#include "arena_map.h"
#include <stdint.h>

#define MAP_LEVEL_COUNT 4ULL
#define MAP_ALIGN_UNIT 4096ULL
#define MAP_ARENA_SLACK 256ULL

/* ---- overflow-checked size arithmetic ---- */

static int checked_add_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (!out || a > UINT64_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

/* floor(value * num / den); division first keeps intermediates in range */
static int fraction_of(uint64_t value, uint64_t num, uint64_t den, uint64_t *out)
{
    uint64_t whole;
    uint64_t rem;
    uint64_t scaled;
    uint64_t extra;

    if (!out || den == 0 || num > den) {
        return 0;
    }
    whole = value / den;
    rem = value % den;
    if (whole != 0 && num > UINT64_MAX / whole) {
        return 0;
    }
    scaled = whole * num;
    if (rem != 0 && num > UINT64_MAX / rem) {
        return 0;
    }
    extra = (rem * num) / den;
    return checked_add_u64(scaled, extra, out);
}

/* rounds *size up to the next 4096 multiple (min 4096): the arena-page
   rule, local to the map because the arena API no longer provides it */
static void map_align_page(uint64_t *size)
{
    if (*size <= MAP_ALIGN_UNIT) {
        *size = MAP_ALIGN_UNIT;
    } else {
        *size += MAP_ALIGN_UNIT - (*size % MAP_ALIGN_UNIT);
    }
}

static int round_up_4096(uint64_t size, uint64_t *out)
{
    uint64_t aligned = size;

    if (!out || size > UINT64_MAX - MAP_ALIGN_UNIT) {
        return 0;
    }
    map_align_page(&aligned);
    if (aligned < MAP_ALIGN_UNIT || (aligned % MAP_ALIGN_UNIT) != 0) {
        return 0;
    }
    *out = aligned;
    return 1;
}

static int map_level_size(uint64_t first_map_arena_size, uint64_t num,
                          uint64_t den, uint64_t *out)
{
    uint64_t raw = 0;

    if (!fraction_of(first_map_arena_size, num, den, &raw)) {
        return 0;
    }
    return round_up_4096(raw, out);
}

/* 100%, 18%, 3%, 0.3% of the first level, each aligned to a page */
static int sum_level_sizes(uint64_t first_map_arena_size, uint64_t *out)
{
    const uint64_t nums[MAP_LEVEL_COUNT] = {100ULL, 18ULL, 3ULL, 3ULL};
    const uint64_t dens[MAP_LEVEL_COUNT] = {100ULL, 100ULL, 100ULL, 1000ULL};
    uint64_t total = 0;
    uint64_t level_size = 0;

    for (uint64_t i = 0; i < MAP_LEVEL_COUNT; i++) {
        if (!map_level_size(first_map_arena_size, nums[i], dens[i], &level_size)) {
            return 0;
        }
        if (!checked_add_u64(total, level_size, &total)) {
            return 0;
        }
    }
    *out = total;
    return 1;
}

static uint64_t token_map_arena_size(uint64_t first_map_arena_size)
{
    uint64_t total = 0;
    uint64_t arena_block = 0;

    if (first_map_arena_size == 0) {
        return 0;
    }
    if (!sum_level_sizes(first_map_arena_size, &total)) {
        return 0;
    }
    if (!round_up_4096((uint64_t)sizeof(Arena), &arena_block)) {
        return 0;
    }
    for (uint64_t i = 0; i < MAP_LEVEL_COUNT; i++) {
        if (!checked_add_u64(total, arena_block, &total)) {
            return 0;
        }
    }
    if (!checked_add_u64(total, MAP_ARENA_SLACK, &total)) {
        return 0;
    }
    if (!round_up_4096(total, &total)) {
        return 0;
    }
    return total;
}

/* advance the bump to the next alignment boundary (no bytes if aligned) */
static int align_arena_bump(Arena *arena, uint64_t alignment)
{
    uint64_t padding;

    if (!arena || !arena->arenaStart || alignment == 0) {
        return 0;
    }
    padding = (uint64_t)(arena->currentSize % alignment);
    if (padding == 0) {
        return 1;
    }
    padding = alignment - padding;
    return arena_alloc(arena, padding, alignof(char)) != NULL;
}

static int align_offset(uint64_t offset, uint64_t alignment, uint64_t *out)
{
    uint64_t mask;

    if (!out || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return 0;
    }
    mask = alignment - 1;
    if (offset > UINT64_MAX - mask) {
        return 0;
    }
    *out = (offset + mask) & ~mask;
    return 1;
}

static int level_prefix_size(uint64_t *offset)
{
    if (!offset) {
        return 0;
    }
    if (!align_offset(*offset, (uint64_t)alignof(Arena), offset)) {
        return 0;
    }
    if (!checked_add_u64(*offset, (uint64_t)sizeof(Arena), offset)) {
        return 0;
    }
    if (!align_offset(*offset, (uint64_t)alignof(Map), offset)) {
        return 0;
    }
    if (!checked_add_u64(*offset, (uint64_t)sizeof(Map), offset)) {
        return 0;
    }
    if (!align_offset(*offset, (uint64_t)alignof(MapEntry), offset)) {
        return 0;
    }
    return 1;
}

/* Layout per level: Arena copy, Map, slot table, then 25% key-copy space. */
static int compute_slot_layout(uint64_t arena_size, uint64_t *entries_offset,
                               uint64_t *slot_count)
{
    uint64_t offset = 0;
    uint64_t key_reserve;
    uint64_t tail;

    if (!entries_offset || !slot_count || arena_size == 0) {
        return 0;
    }
    key_reserve = arena_size / 4;
    if (arena_size < key_reserve) {
        return 0;
    }
    if (!level_prefix_size(&offset)) {
        return 0;
    }
    tail = arena_size - key_reserve;
    if (offset > tail) {
        return 0;
    }
    *entries_offset = offset;
    *slot_count = (tail - offset) / (uint64_t)sizeof(MapEntry);
    return 1;
}

static int allocate_level_storage(Arena *child, uint64_t slot_count,
                                  Arena **arena_copy, Map **level,
                                  MapEntry **entries)
{
    uint64_t slot_bytes;
    Arena *copy;
    Map *map;
    MapEntry *table;

    if (!child || !arena_copy || !level || !entries) {
        return 0;
    }
    if (slot_count > UINT64_MAX / (uint64_t)sizeof(MapEntry)) {
        return 0;
    }
    slot_bytes = slot_count * (uint64_t)sizeof(MapEntry);
    copy = (Arena *)arena_alloc(child, (uint64_t)sizeof(Arena), alignof(Arena));
    map = (Map *)arena_alloc(child, (uint64_t)sizeof(Map), alignof(Map));
    table = (MapEntry *)arena_alloc(child, slot_bytes, alignof(MapEntry));
    if (!copy || !map || !table) {
        return 0;
    }
    *arena_copy = copy;
    *level = map;
    *entries = table;
    return 1;
}

static void init_map_level(Map *level, MapEntry *entries, uint64_t slot_count,
                           uint64_t arena_size, Arena *token_map_arena)
{
    level->entries = entries;
    level->slot_count = slot_count;
    level->used_count = 0;
    level->arena_size = arena_size;
    level->token_map_arena = token_map_arena;
    level->next = NULL;
    for (uint64_t i = 0; i < slot_count; i++) {
        entries[i].key.value = NULL;
        entries[i].key.size = 0;
        entries[i].value.value = NULL;
        entries[i].value.size = 0;
        entries[i].used = 0;
    }
}

static Map *create_map_level(Arena *token_map_arena, uint64_t arena_size)
{
    uint64_t entries_offset = 0;
    uint64_t slot_count = 0;
    Arena child;
    Arena *arena_copy = NULL;
    Map *level = NULL;
    MapEntry *entries = NULL;

    if (!token_map_arena || !token_map_arena->arenaStart) return NULL;
    if (arena_size == 0 || (arena_size % MAP_ALIGN_UNIT) != 0) return NULL;
    if (!align_arena_bump(token_map_arena, MAP_ALIGN_UNIT)) return NULL;
    child = make_new_children_arena(token_map_arena, arena_size);
    if (!child.arenaStart || (uint64_t)child.capacity != arena_size) return NULL;
    if (!compute_slot_layout(arena_size, &entries_offset, &slot_count) ||
        slot_count == 0) return NULL;
    if (!allocate_level_storage(&child, slot_count, &arena_copy, &level, &entries)) {
        return NULL;
    }
    if ((uint64_t)((char *)entries - child.arenaStart) != entries_offset) return NULL;
    /* Arena copy sits immediately before Map; push recovers it there. */
    if ((uint64_t)((char *)level - (char *)arena_copy) != (uint64_t)sizeof(Arena)) {
        return NULL;
    }
    *arena_copy = child;
    init_map_level(level, entries, slot_count, arena_size, token_map_arena);
    return level;
}

static int first_level_size_checked(uint64_t requested, uint64_t *out)
{
    uint64_t aligned;
    uint64_t entries_offset = 0;
    uint64_t slot_count = 0;

    if (!out || requested == 0 || requested > UINT64_MAX - MAP_ALIGN_UNIT) {
        return 0;
    }
    aligned = requested;
    map_align_page(&aligned);
    if (aligned < MAP_ALIGN_UNIT || (aligned % MAP_ALIGN_UNIT) != 0) {
        return 0;
    }
    if (!compute_slot_layout(aligned, &entries_offset, &slot_count) || slot_count == 0) {
        return 0;
    }
    *out = aligned;
    return 1;
}

Map *init_token_map_arena(Arena *father_arena, uint64_t first_map_arena_size)
{
    uint64_t first_size = 0;
    uint64_t total_size;
    Arena token_map_arena;
    Arena *stored_arena;

    if (!father_arena || !father_arena->arenaStart) return NULL;
    if (!first_level_size_checked(first_map_arena_size, &first_size)) return NULL;
    total_size = token_map_arena_size(first_map_arena_size);
    if (total_size == 0) return NULL;
    if (!align_arena_bump(father_arena, MAP_ALIGN_UNIT)) return NULL;
    token_map_arena = make_new_children_arena(father_arena, total_size);
    if (!token_map_arena.arenaStart ||
        (uint64_t)token_map_arena.capacity != total_size ||
        ((uintptr_t)token_map_arena.arenaStart % MAP_ALIGN_UNIT) != 0) return NULL;
    stored_arena = (Arena *)arena_alloc(&token_map_arena, (uint64_t)sizeof(Arena),
                                        alignof(Arena));
    if (!stored_arena) return NULL;
    *stored_arena = token_map_arena;
    return create_map_level(stored_arena, first_size);
}

/* ---- map_push ---- */

/* W3 helpers, defined below in this file */
static uint64_t key_byte_sum(Token key);
static int keys_equal(Token a, Token b);

/* 18%, 3%, 0.3% of the FIRST level's size; the result is rounded up to a
   page boundary because create_map_level only accepts 4096-aligned level
   sizes, and the permille product is overflow-checked before it is formed. */
static uint64_t w2_overflow_arena_size(uint64_t first_size, uint64_t depth)
{
    uint64_t permille;
    uint64_t whole;
    uint64_t size;

    if (depth == 2) {
        permille = 180; /* 18% */
    } else if (depth == 3) {
        permille = 30; /* 3% */
    } else if (depth == 4) {
        permille = 3; /* 0.3% */
    } else {
        return 0;
    }
    if (first_size / 1000 > UINT64_MAX / permille) {
        return 0;
    }
    whole = (first_size / 1000) * permille;
    if (whole > UINT64_MAX - ((first_size % 1000) * permille) / 1000) {
        return 0;
    }
    size = whole + ((first_size % 1000) * permille) / 1000;
    map_align_page(&size);
    return size;
}

static int w2_copy_key(Map *level, Token key, Token *out)
{
    Arena *level_arena = (Arena *)((char *)level - sizeof(Arena));
    char *dst;

    if (key.size == 0) {
        out->value = (char *)level_arena;
        out->size = 0;
        return 1;
    }
    if (key.value == NULL) {
        return 0;
    }
    dst = (char *)arena_alloc(level_arena, key.size, 1);
    if (dst == NULL) {
        return 0;
    }
    for (uint64_t i = 0; i < key.size; i++) {
        dst[i] = key.value[i];
    }
    out->value = dst;
    out->size = key.size;
    return 1;
}

static int w2_push_in_level(Map *level, Token key, Token value)
{
    uint64_t idx;
    MapEntry *entry;
    Token copy;

    if (level == NULL || level->entries == NULL || level->slot_count == 0) {
        return 0;
    }
    idx = key_byte_sum(key) % level->slot_count;
    entry = &level->entries[idx];
    if (entry->used != 0) {
        if (key.size == entry->key.size && keys_equal(key, entry->key) != 0) {
            entry->value = value; /* replace whole value Token, no allocation */
            return 1;
        }
        return 0;
    }
    if (w2_copy_key(level, key, &copy) == 0) {
        return 0;
    }
    entry->key = copy;
    entry->value = value; /* shallow copy; value bytes are not owned by the map */
    entry->used = 1;
    level->used_count++;
    return 1;
}

static Map *w2_make_next_level(Map *map, uint64_t depth)
{
    uint64_t size = w2_overflow_arena_size(map->arena_size, depth);

    if (size == 0) {
        return NULL;
    }
    return create_map_level(map->token_map_arena, size);
}

int map_push(Map *map, Token key, Token value)
{
    Map *level = map;
    uint64_t depth;

    /* only the key is validated; value contents are stored as given */
    if (map == NULL || map->entries == NULL || map->slot_count == 0 ||
        (key.value == NULL && key.size > 0)) {
        return 0;
    }
    for (depth = 1; level != NULL; depth++) {
        if (w2_push_in_level(level, key, value) != 0) {
            return 1;
        }
        if (depth >= MAP_LEVEL_COUNT) {
            return 0;
        }
        if (level->next == NULL) {
            level->next = w2_make_next_level(map, depth + 1);
            if (level->next == NULL) {
                return 0;
            }
        }
        level = level->next;
    }
    return 0;
}

/* ---- map_search + key helpers ---- */

static uint64_t key_byte_sum(Token key)
{
    uint64_t sum = 0;
    uint64_t i;

    if (key.value == NULL) {
        return 0;
    }
    for (i = 0; i < key.size; i++) {
        sum += (uint64_t)(unsigned char)key.value[i];
    }
    return sum;
}

static int keys_equal(Token a, Token b)
{
    uint64_t i;

    if (a.size != b.size) {
        return 0;
    }
    if (a.size == 0) {
        return 1;
    }
    if (a.value == NULL || b.value == NULL) {
        return 0;
    }
    for (i = 0; i < a.size; i++) {
        if ((unsigned char)a.value[i] != (unsigned char)b.value[i]) {
            return 0;
        }
    }
    return 1;
}

static MapEntry *map_find_level(Map *level, Token key)
{
    uint64_t idx;
    MapEntry *entry;

    if (level == NULL || level->entries == NULL || level->slot_count == 0) {
        return NULL;
    }
    idx = key_byte_sum(key) % level->slot_count;
    entry = &level->entries[idx];
    if (!entry->used) {
        return NULL;
    }
    if (!keys_equal(entry->key, key)) {
        return NULL;
    }
    return entry;
}

Token *map_search(Map *map, Token key)
{
    Map *level;
    MapEntry *entry;

    if (map == NULL) {
        return NULL;
    }
    if (key.value == NULL && key.size > 0) {
        return NULL;
    }
    level = map;
    while (level != NULL) {
        entry = map_find_level(level, key);
        if (entry != NULL) {
            return &entry->value;
        }
        level = level->next;
    }
    return NULL;
}
