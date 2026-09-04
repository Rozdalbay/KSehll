#pragma once

#ifndef KSHELL_LEXER_H
#define KSHELL_LEXER_H

#include <string>
#include <vector>

#include "parser/Token.h"

namespace kshell
{

class Lexer
{
public:
    explicit Lexer(std::wstring input);

    std::vector<Token> tokenize();

private:
    wchar_t peek(size_t offset = 0) const;
    wchar_t advance();
    void skipWhitespace();
    void skipTrailingWhitespace();

    std::wstring readQuoted(wchar_t quoteChar);
    std::wstring readWord();
    std::wstring readVariable();
    std::wstring readPercentVariable();

    std::wstring input_;
    size_t pos_ = 0;
};

} // namespace kshell

#endif // KSHELL_LEXER_H
