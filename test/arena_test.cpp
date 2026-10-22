#include "../src/arena/arena.h"
#include <cstddef>
#include <cstdio>
#include <gtest/gtest.h>


TEST(Arena, ArenaAllocationTest) {
    Arena father = init_father_arena();
    constexpr size_t childCount = 4;
    const size_t capacities[childCount] = {
        MB_4,
        2 * MB_4,
        4 * MB_4,
        GB_1
    };
    const size_t sizes[childCount] = {
        64,
        2 * MB_4,
        129,
        GB_1 + 1
    };
    Arena children[childCount] = {};
    uint arenaCapacityOverflowAllocationTestIndex = 3; 
    for (size_t i = 0; i < childCount; ++i) {
        children[i] = make_new_children_arena(&father, capacities[i]);
        
        ASSERT_EQ(children[i].capacity, capacities[i]);
        ASSERT_NE(children[i].arenaStart, nullptr);
        children[i].arenaStart[capacities[i] - 1] = 'x';
        char* ptr = (char*) arena_alloc(&children[i], sizes[i], alignof(char));
        if (i == arenaCapacityOverflowAllocationTestIndex) {
            EXPECT_EQ(ptr, nullptr);
        }
        else {
            EXPECT_NE(ptr, nullptr);
        }
       
        EXPECT_EQ(children[i].arenaStart[capacities[i] - 1], 'x');
    }

    EXPECT_EQ(arena_free(&father), 1);
}
TEST(Arena, ArenaFreeTest) {
    Arena a = init_father_arena();
    int isFreeArenaCompleted = arena_free(&a);
    EXPECT_EQ(isFreeArenaCompleted, 1);
    isFreeArenaCompleted = arena_free(&a);
    EXPECT_EQ(isFreeArenaCompleted, 0);
}
