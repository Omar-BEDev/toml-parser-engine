#include "arena.h"
#include <cstddef>
#include <cstdlib>

ArenaNode init_arena_head_node() {
    ArenaNode an = {
        .head = NULL,
        .length = 0
    };
    return an;
}
Arena init_father_arena() {
    Arena a = {
    .arenaStart = (char*)allocate_memory(),
    .currentSize = 0,
    .capacity = GB_64,
    .next = NULL
    };
    return a;
}

Arena init_children_arena(unsigned long long childrenSize) {
    Arena a = {
    .arenaStart = NULL,
    .currentSize = 0,
    .capacity = childrenSize,
    .next = NULL
    };
    return a;
}

Arena* search_last_arena_node(ArenaNode *an) {
    if (!an->head) return NULL;
    uint i = 0;
    Arena *currentArena = an->head;
    while(i < an->length) {
        if (!currentArena->next) {
            return currentArena;
        }
        currentArena = currentArena->next;
    }
    return currentArena;
}

Arena make_new_children_arena(Arena *father_arena, unsigned long long childrenArenaSize) {
    bool isAllocated = allocate_arena_memory(father_arena, childrenArenaSize);
    if (!isAllocated) {
        Arena a = init_children_arena(0);
        return a;
    }
    Arena a = init_children_arena(childrenArenaSize);
    a.arenaStart = (char*) arena_alloc(father_arena, childrenArenaSize, alignof(char));
    return a;
}

void* arena_alloc(Arena *a, size_t size, size_t align) {
    if (!a->arenaStart) {
        return NULL;
    }
    Arena *currentArena = a;
    size_t padding = a->currentSize % align != 0? align - (a->currentSize % align) : 0;
    if (a->currentSize + size + padding> a->capacity) {
      return NULL; 
    }
  
    void *ptr = a->arenaStart + a->currentSize + padding;
    a->currentSize += padding + size;
    return ptr;
}

int arena_free(Arena *father_arena) {
    if (!father_arena->arenaStart) return false;
    bool isFreeCompletd = free_memory(father_arena->arenaStart);
    if (isFreeCompletd == false) return isFreeCompletd;
    *father_arena = {
        .arenaStart =NULL,
        .currentSize = 0
    };
    return isFreeCompletd;
}