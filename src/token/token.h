#ifndef TOKEN
#define TOKEN

#include <cstdint>

/* the lexer stores a variable name/value lexeme as pointer + byte size
   (not NUL-terminated) */
typedef struct Token {
    char* value;
    uint64_t size;
} Token;

Token init_token(char* value_begin, uint64_t size);

#endif
