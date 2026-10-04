#ifndef ARENA
#define ARENA

#include <cstddef>
typedef struct Arena {
    char* arenaStart;
    size_t currentSize;
    size_t capacity;
} Arena;

void* arena_alloc(Arena *a, size_t size);
void arena_free(Arena *a, void** arena);
#endif