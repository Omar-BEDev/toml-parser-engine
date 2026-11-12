#include "../src/arena/arena_map.h"
#include <gtest/gtest.h>
#include <cstring>

namespace {

/* keys/values built on the stack or as literals; the map copies key bytes
   and stores the value Token by value (shallow) */
Token tok(const char *s) {
    return init_token((char *)s, (uint64_t)std::strlen(s));
}

bool token_bytes_eq(const Token *t, const char *expected) {
    size_t len = std::strlen(expected);
    return t != nullptr && t->size == len &&
           std::memcmp(t->value, expected, len) == 0;
}

/* two-char keys that all share byte sum 200 -> guaranteed same bucket:
   'a'+i paired with the byte that completes the sum */
void same_sum_key(uint64_t i, char *out) {
    out[0] = (char)('a' + (char)(i % 26));
    out[1] = (char)(200 - out[0]);
    out[2] = '\0';
}

TEST(ArenaMapTest, PushSearchBasic) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_search(map, tok("server")), nullptr);
    EXPECT_EQ(map_push(map, tok("server"), tok("my_database")), 1);

    Token *value = map_search(map, tok("server"));
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(token_bytes_eq(value, "my_database"));
    EXPECT_EQ(map_search(map, tok("absent")), nullptr);
    EXPECT_EQ(map->used_count, 1u);
    EXPECT_EQ(map->next, nullptr);
}

TEST(ArenaMapTest, ReplaceSameKeyKeepsOneSlot) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(map, tok("temp_max"), tok("40")), 1);
    EXPECT_EQ(map_push(map, tok("temp_max"), tok("-5")), 1);

    Token *value = map_search(map, tok("temp_max"));
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(token_bytes_eq(value, "-5"));
    EXPECT_EQ(map->used_count, 1u);
    EXPECT_EQ(map->next, nullptr);
}

/* owner's example: "cat" and "act" share byte sum 312 -> the second key
   must land in a lazily created overflow level (18% of the first map) */
TEST(ArenaMapTest, CollisionGoesToOverflowLevel) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(map, tok("cat"), tok("animal")), 1);
    EXPECT_EQ(map_push(map, tok("act"), tok("verb")), 1);

    Token *cat = map_search(map, tok("cat"));
    Token *act = map_search(map, tok("act"));
    EXPECT_TRUE(token_bytes_eq(cat, "animal"));
    EXPECT_TRUE(token_bytes_eq(act, "verb"));
    EXPECT_NE(map->next, nullptr);
    EXPECT_EQ(map->next->used_count, 1u);
}

/* same-key replace must hit the overflow level where the key lives */
TEST(ArenaMapTest, ReplaceKeyStoredInOverflowLevel) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(map, tok("cat"), tok("animal")), 1);
    EXPECT_EQ(map_push(map, tok("act"), tok("verb")), 1);
    EXPECT_EQ(map_push(map, tok("act"), tok("action")), 1);

    Token *act = map_search(map, tok("act"));
    ASSERT_NE(act, nullptr);
    EXPECT_TRUE(token_bytes_eq(act, "action"));
    EXPECT_EQ(map->next->used_count, 1u);
    EXPECT_TRUE(token_bytes_eq(map_search(map, tok("cat")), "animal"));
}

/* keys with identical byte sums all want the same slot of every level:
   level 1..4 each take one var, the 5th fails (0.3% level is the last) */
TEST(ArenaMapTest, ChainFillsThenExhausts) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);
    const char *values[7] = {"v0", "v1", "v2", "v3", "v4", "v5", "v6"};
    char key[3];
    Map *level;

    ASSERT_NE(map, nullptr);
    for (uint64_t i = 0; i < 4; i++) {
        same_sum_key(i, key);
        EXPECT_EQ(map_push(map, tok(key), tok(values[i])), 1) << "i=" << i;
    }
    for (uint64_t i = 4; i < 7; i++) {
        same_sum_key(i, key);
        EXPECT_EQ(map_push(map, tok(key), tok(values[i])), 0) << "i=" << i;
    }
    for (uint64_t i = 0; i < 4; i++) {
        same_sum_key(i, key);
        Token *value = map_search(map, tok(key));
        ASSERT_NE(value, nullptr) << "i=" << i;
        EXPECT_TRUE(token_bytes_eq(value, values[i])) << "i=" << i;
    }
    same_sum_key(5, key);
    EXPECT_EQ(map_search(map, tok(key)), nullptr);

    level = map;
    for (int i = 0; i < 4; i++) {
        ASSERT_NE(level, nullptr);
        EXPECT_EQ(level->used_count, 1u);
        level = level->next;
    }
    EXPECT_EQ(level, nullptr);
}

TEST(ArenaMapTest, EmptyKeyIsLegal) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(map, tok(""), tok("empty")), 1);

    Token *value = map_search(map, tok(""));
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(token_bytes_eq(value, "empty"));
    EXPECT_EQ(map_push(map, tok(""), tok("replaced")), 1);
    EXPECT_TRUE(token_bytes_eq(map_search(map, tok("")), "replaced"));
}

TEST(ArenaMapTest, InvalidArgumentsAreRejected) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);
    Token bad_key = init_token(nullptr, 5);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(nullptr, tok("k"), tok("v")), 0);
    EXPECT_EQ(map_search(nullptr, tok("k")), nullptr);
    EXPECT_EQ(map_push(map, bad_key, tok("v")), 0);
    EXPECT_EQ(map_search(map, bad_key), nullptr);

    EXPECT_EQ(init_token_map_arena(nullptr, 4096), nullptr);
    EXPECT_EQ(init_token_map_arena(&father, 0), nullptr);
}

/* value contents are stored as given, even a malformed value Token */
TEST(ArenaMapTest, ValueIsStoredUnchecked) {
    Arena father = init_father_arena();
    Map *map = init_token_map_arena(&father, 4096);
    Token empty_value = init_token(nullptr, 3);

    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map_push(map, tok("log_path"), empty_value), 1);
    Token *value = map_search(map, tok("log_path"));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, nullptr);
    EXPECT_EQ(value->size, 3u);
}

} /* namespace */
