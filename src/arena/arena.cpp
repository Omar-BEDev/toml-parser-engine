#include "arena.h"
#include <cstddef>
#include <cstdlib>

size_t arenaSize = 8192;
void* arena = malloc(arenaSize);
Arena a = {
    .arenaStart = (char*)arena,
    .currentSize = 0,
    .capacity = arenaSize
};

void* arena_alloc(Arena *a, size_t size) {
    size_t padding = a->currentSize % 8 != 0? 8 - (a->currentSize % 8) : 0;
    if (a->currentSize + size + padding> a->capacity) {
        return NULL;
    }
    
    void *ptr = a->arenaStart + a->currentSize + padding;
    a->currentSize += padding + size;
    return ptr;
}

void arena_free(Arena *a, void** arena) {
    free(*arena);
    *a = {
        .arenaStart =NULL,
        .currentSize = 0
    };
}