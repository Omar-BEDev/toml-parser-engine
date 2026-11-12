#include "token.h"

Token init_token(char* value_begin, uint64_t size) {
    Token t = {
        .value = value_begin,
        .size = size
    };
    return t;
}
