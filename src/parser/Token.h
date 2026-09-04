#pragma once

#ifndef KSHELL_TOKEN_H
#define KSHELL_TOKEN_H

#include <string>

namespace kshell
{

enum class TokenType
{
    Word,
    Pipe,
    Less,
    Greater,
    DoubleGreater,
    Error2Greater,
    ErrorAppend,
    ErrorRedirect,
    ErrorOutput,
    BothOutput,
    Background,
    And,
    Or,
    Semicolon,
    Variable,
    End,
    Invalid
};

struct Token
{
    TokenType type = TokenType::Word;
    std::wstring value;
    size_t position = 0;
};

} // namespace kshell

#endif // KSHELL_TOKEN_H
