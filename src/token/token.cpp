#include "token.h"
#include <iostream>
#include <string_view>
#include <unordered_map>

std::unordered_map<std::string_view, TokenType> symbols = {
    {"=", ASSIGMENT},
    {"[", OPEN_BRACES},
    {"]", CLOSE_BRACES},
    {"\"", DOUBLE_QUOTATION},
    {"#", HASHTAG},
    {".", POINT},
    {"{", OPEN_CURLY_BRACES},
    {"}", CLOSE_CURLY_BRACES},
    {"'", QUOTATION},
    {"\\", BACKSLACH},
    {"_", UUNDERSCORE},
    {":", COLON},
    {"+", ADDITION_S},
    {"-", SUBSTRACTION_S},
};

TokenType findTokenType(std::string_view symbol) {
    auto it = symbols.find(symbol);
    if (it == symbols.end()) {
        return NOT_FOUND;
    }
    return it->second;
}
