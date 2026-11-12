#ifndef ARENA_MAP
#define ARENA_MAP
#include <stdint.h>
#include <stddef.h>
#include "../token/token.h"
#include "arena.h"

/* one map level: fixed slot table + key-copy area, all arena memory */
typedef struct MapEntry {
    Token key;        /* var name, copied into this level's arena */
    Token value;      /* var value, shallow copy (bytes not owned) */
    uint8_t used;     /* slot occupied flag */
} MapEntry;

typedef struct Map {
    MapEntry *entries;          /* slot table of this level */
    uint64_t slot_count;        /* slots in this level */
    uint64_t used_count;        /* occupied slots */
    uint64_t arena_size;        /* bytes of this level's arena */
    Arena *token_map_arena;     /* arena that owns all levels (for lazy overflow) */
    Map *next;                  /* next overflow level (18% -> 3% -> 0.3%) */
} Map;

/* Creates the token map arena (child of father) + the first map level.
   tokenMapArena must hold ALL possible levels, so its size is computed
   from first_map_arena_size. Returns NULL on failure. */
Map *init_token_map_arena(Arena *father_arena, uint64_t first_map_arena_size);

/* Stores var name -> var value. The key bytes are copied into the level
   arena; the value Token is stored by value (shallow) so its bytes must
   outlive the map (e.g. file-mapping content). On an existing key only
   the value Token is overwritten (no allocation). Returns 1 on success
   (stored or replaced), 0 on failure (all levels full / invalid input). */
int map_push(Map *map, Token key, Token value);

/* Returns the stored var value Token for the key, or NULL if absent.
   The returned pointer stays valid for the map's lifetime. */
Token *map_search(Map *map, Token key);
#endif
