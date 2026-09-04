#pragma once

#ifndef KSHELL_PARSER_H
#define KSHELL_PARSER_H

#include <string>
#include <vector>
#include <optional>

#include "parser/Command.h"
#include "parser/Token.h"

namespace kshell
{

struct ParseError
{
    std::wstring message;
    size_t position = 0;
};

class Parser
{
public:
    explicit Parser(std::vector<Token> tokens);

    std::vector<Pipeline> parse(std::optional<ParseError>& error);

private:
    Command parseCommand(size_t& index, bool& background, std::optional<ParseError>& error);
    bool parseRedirection(size_t& index, Command& command, std::optional<ParseError>& error);
    std::wstring parseRedirectTarget(size_t& index, std::optional<ParseError>& error);
    RedirectType mapRedirectType(TokenType type) const;

    std::vector<Token> tokens_;
    size_t current_ = 0;
};

} // namespace kshell

#endif // KSHELL_PARSER_H
