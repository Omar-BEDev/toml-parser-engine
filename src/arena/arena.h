#ifndef ARENA
#define ARENA

#define GB_64 (64ULL * 1024 * 1024 * 1024)
#define MB_4 (4ULL * 1024 * 1024)
#define GB_1 (1ULL * 1024 * 1024 * 1024)
#include <cstddef>
#include <cstddef>
#include <sys/types.h>

typedef struct Arena {
    char* arenaStart;
    size_t currentSize;
    size_t capacity;
    Arena* next;
} Arena;

typedef struct ArenaNode {
    Arena* head;
    uint length;
} ArenaNode;

/*
* this design is a lifetime of memory management of program 
* one malloc and one free with the arenas spaces for diffrent uses

*/
#ifdef _WIN32
    #include <windows.h>
#else 
#include <sys/mman.h>
#endif

inline void* allocate_memory() {
    #ifdef _WIN32 
        return VirtualAlloc(NULL, GB_64, MEM_RESERVE, PAGE_NOACCESS);
    #else 
        return mmap(NULL, GB_64, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    #endif
return NULL;
}

inline int allocate_arena_memory(Arena *father_arena, size_t arenaSize) {
    if (!father_arena || !father_arena->arenaStart ||
        // to ignore capcity overflow in father arena 
        father_arena->currentSize + arenaSize > father_arena->capacity ||
        arenaSize > father_arena->capacity - father_arena->currentSize) {
        return 0;
    }
    #ifdef _WIN32
        DWORD old_protect;
        return VirtualProtect(father_arena->arenaStart + father_arena->currentSize,
                              arenaSize, PAGE_READWRITE, &old_protect);
    #else
        int result = mprotect(father_arena->arenaStart + father_arena->currentSize,
                              arenaSize, PROT_READ | PROT_WRITE);
        if (result == 0) {
            return 1;
        }
        else return 0;
    #endif
}
inline int free_memory(void* addr) {
    #ifdef _WIN32
        return VirtualFree(addr, 0, MEM_RELEASE);
    #else
        int isMunmapCompleted =  munmap(addr, GB_64);
        if (isMunmapCompleted == 0) {
            return 1;
        }
        else return 0;
    #endif
    return 0;
}

void* arena_alloc(Arena *a, size_t size, size_t align);
int arena_free(Arena *a);
Arena init_father_arena();
ArenaNode init_arena_head_node();
Arena make_new_children_arena(Arena *father_arena, unsigned long long childrenArenaSize);
#endif