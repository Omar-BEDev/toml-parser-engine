#ifndef TOKEN
#define TOKEN

#include <string_view>

typedef enum TokenType {
    ASSIGMENT,
    OPEN_BRACES,
    CLOSE_BRACES,
    DOUBLE_QUOTATION,
    HASHTAG,
    POINT,
    OPEN_CURLY_BRACES,
    CLOSE_CURLY_BRACES,
    QUOTATION,
    BACKSLACH,
    UUNDERSCORE,
    COLON,
    // because is just symbols for history not for mathematics operation ------
    ADDITION_S,
    SUBSTRACTION_S,
    // ------------------------------------------------------------------------
    NOT_FOUND
} TokenType;


typedef struct Token {
    std::string_view value;
    TokenType tokenType;
} Token;


#endif