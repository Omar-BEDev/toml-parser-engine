#ifndef TOKEN
#define TOKEN

#include <string_view>
typedef enum TokenType {

} TokenType;


typedef struct Token {
    std::string_view value;
    TokenType tokenType;
} Token;


#endif