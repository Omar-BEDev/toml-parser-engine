#include "../src/arena/arena.h"
#include <cstdio>
#include <gtest/gtest.h>


TEST(Arena, ArenaAllocationTest) {
    Arena a = init_father_arena();
    Arena children_a = make_new_children_arena(&a, MB_4);
    
    EXPECT_EQ(children_a.capacity, MB_4);
    EXPECT_NE(children_a.arenaStart, nullptr);
}
TEST(Arena, ArenaFreeTest) {
     Arena a = init_father_arena();
    int isFreeArenaCompleted = arena_free(&a);
    EXPECT_EQ(isFreeArenaCompleted, 1);
    isFreeArenaCompleted = arena_free(&a);
    EXPECT_EQ(isFreeArenaCompleted, 0);
}

