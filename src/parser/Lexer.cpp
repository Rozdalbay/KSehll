#include "parser/Lexer.h"

#include <cwctype>

namespace kshell
{

Lexer::Lexer(std::wstring input)
    : input_(std::move(input))
{
}

wchar_t Lexer::peek(size_t offset) const
{
    const size_t index = pos_ + offset;
    if (index < input_.size())
    {
        return input_[index];
    }
    return L'\0';
}

wchar_t Lexer::advance()
{
    if (pos_ < input_.size())
    {
        return input_[pos_++];
    }
    return L'\0';
}

void Lexer::skipWhitespace()
{
    while (pos_ < input_.size() && std::iswspace(static_cast<wint_t>(input_[pos_])))
    {
        ++pos_;
    }
}

void Lexer::skipTrailingWhitespace()
{
    while (pos_ < input_.size() &&
           (std::iswspace(static_cast<wint_t>(input_[pos_])) || input_[pos_] == L'#'))
    {
        ++pos_;
    }
}

std::wstring Lexer::readQuoted(wchar_t quoteChar)
{
    advance();
    std::wstring result;
    bool escaped = false;

    while (pos_ < input_.size())
    {
        const wchar_t c = advance();

        if (escaped)
        {
            if (c == quoteChar || c == L'\\')
            {
                result.push_back(c);
            }
            else
            {
                result.push_back(L'\\');
                result.push_back(c);
            }
            escaped = false;
            continue;
        }

        if (c == L'\\' && quoteChar == L'"')
        {
            escaped = true;
            continue;
        }

        if (c == quoteChar)
        {
            break;
        }

        result.push_back(c);
    }
    return result;
}

std::wstring Lexer::readWord()
{
    std::wstring result;
    bool escaped = false;

    while (pos_ < input_.size())
    {
        const wchar_t c = peek();

        if (escaped)
        {
            result.push_back(c);
            ++pos_;
            escaped = false;
            continue;
        }

        if (c == L'\\')
        {
            ++pos_;
            escaped = true;
            continue;
        }

        if (std::iswspace(static_cast<wint_t>(c)) ||
            c == L'|' || c == L'&' || c == L';' ||
            c == L'<' || c == L'>' || c == L'%')
        {
            break;
        }

        if (c == L'"' || c == L'\'')
        {
            result += readQuoted(c);
            continue;
        }

        if (c == L'$')
        {
            const wchar_t next = peek(1);
            if (next == L'(' || next == L' ' || next == L'\t' || next == L'\0')
            {
                result.push_back(advance());
            }
            else
            {
                result += readVariable();
            }
            continue;
        }

        result.push_back(advance());
    }

    if (escaped)
    {
        result.pop_back();
    }
    return result;
}

std::wstring Lexer::readVariable()
{
    ++pos_;
    std::wstring name;
    while (pos_ < input_.size())
    {
        const wchar_t c = peek();
        if (std::iswalnum(static_cast<wint_t>(c)) || c == L'_')
        {
            name.push_back(advance());
        }
        else
        {
            break;
        }
    }
    return L"$" + name;
}

std::wstring Lexer::readPercentVariable()
{
    ++pos_;
    std::wstring name;
    while (pos_ < input_.size())
    {
        const wchar_t c = peek();
        if (c == L'%')
        {
            ++pos_;
            return L"%" + name + L"%";
        }
        name.push_back(advance());
    }
    return L"%" + name;
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;
    skipWhitespace();

    while (pos_ < input_.size())
    {
        const wchar_t c = peek();
        const size_t tokenPos = pos_;

        if (std::iswspace(static_cast<wint_t>(c)))
        {
            skipWhitespace();
            continue;
        }

        Token token;
        token.position = tokenPos;

        if (c == L'|')
        {
            token.type = TokenType::Pipe;
            ++pos_;
        }
        else if (c == L'&')
        {
            if (peek(1) == L'&')
            {
                token.type = TokenType::And;
                pos_ += 2;
            }
            else
            {
                token.type = TokenType::Background;
                ++pos_;
            }
        }
        else if (c == L';')
        {
            token.type = TokenType::Semicolon;
            ++pos_;
        }
        else if (c == L'<')
        {
            token.type = TokenType::Less;
            ++pos_;
        }
        else if (c == L'>')
        {
            if (peek(1) == L'>')
            {
                token.type = TokenType::DoubleGreater;
                pos_ += 2;
            }
            else if (peek(1) == L'&')
            {
                token.type = TokenType::ErrorRedirect;
                pos_ += 2;
            }
            else
            {
                token.type = TokenType::Greater;
                ++pos_;
            }
        }
        else if (c == L'@')
        {
            token.type = TokenType::Word;
            token.value = L"@";
            ++pos_;
        }
        else if (c == L'%')
        {
            if (peek(1) == L'%')
            {
                token.type = TokenType::Word;
                token.value = readPercentVariable();
            }
            else
            {
                token.type = TokenType::Word;
                token.value = readPercentVariable();
            }
        }
        else if (c == L'2' && peek(1) == L'>')
        {
            if (peek(2) == L'>')
            {
                token.type = TokenType::ErrorAppend;
                pos_ += 3;
            }
            else if (peek(2) == L'&')
            {
                token.type = TokenType::BothOutput;
                pos_ += 3;
            }
            else
            {
                token.type = TokenType::ErrorOutput;
                pos_ += 2;
            }
        }
        else if (c == L'"' || c == L'\'')
        {
            token.type = TokenType::Word;
            token.value = readQuoted(c);
        }
        else if (c == L'$')
        {
            const wchar_t next = peek(1);
            if (next == L'(' || next == L' ' || next == L'\t' || next == L'\0')
            {
                token.type = TokenType::Word;
                token.value.push_back(advance());
            }
            else
            {
                token.type = TokenType::Variable;
                token.value = readVariable();
            }
        }
        else
        {
            token.type = TokenType::Word;
            token.value = readWord();
        }

        if (token.type != TokenType::Invalid || !token.value.empty())
        {
            tokens.push_back(std::move(token));
        }

        skipTrailingWhitespace();
    }

    Token end;
    end.type = TokenType::End;
    tokens.push_back(end);
    return tokens;
}

} // namespace kshell
