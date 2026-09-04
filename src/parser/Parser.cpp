#include "parser/Parser.h"

namespace kshell
{

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens))
{
}

std::vector<Pipeline> Parser::parse(std::optional<ParseError>& error)
{
    std::vector<Pipeline> pipelines;
    Pipeline current;

    while (current_ < tokens_.size())
    {
        const Token& token = tokens_[current_];

        switch (token.type)
        {
        case TokenType::End:
            if (!current.commands.empty())
            {
                pipelines.push_back(std::move(current));
            }
            return pipelines;

        case TokenType::Semicolon:
            if (!current.commands.empty())
            {
                pipelines.push_back(std::move(current));
                current = Pipeline();
            }
            ++current_;
            break;

        case TokenType::Pipe:
            if (current.commands.empty())
            {
                error = ParseError{L"Unexpected '|'", token.position};
                return {};
            }
            ++current_;
            break;

        case TokenType::Background:
        {
            if (current.commands.empty())
            {
                error = ParseError{L"Unexpected '&'", token.position};
                return {};
            }
            current.background = true;
            ++current_;
            if (current_ < tokens_.size() && tokens_[current_].type == TokenType::End)
            {
                pipelines.push_back(std::move(current));
                current = Pipeline();
            }
            else if (current_ < tokens_.size() && tokens_[current_].type == TokenType::Semicolon)
            {
                pipelines.push_back(std::move(current));
                current = Pipeline();
                ++current_;
            }
            break;
        }

        case TokenType::Word:
        case TokenType::Variable:
        case TokenType::Less:
        case TokenType::Greater:
        case TokenType::DoubleGreater:
        case TokenType::ErrorRedirect:
        case TokenType::ErrorOutput:
        case TokenType::ErrorAppend:
        case TokenType::BothOutput:
        {
            bool background = false;
            Command cmd = parseCommand(current_, background, error);
            if (error)
            {
                return {};
            }
            if (cmd.program.empty() && !cmd.redirects.empty())
            {
                current.commands.push_back(std::move(cmd));
            }
            else
            {
                size_t cmdIndex = 0;
                bool foundCombined = false;
                (void)cmdIndex;
                (void)foundCombined;
                current.commands.push_back(std::move(cmd));
            }
            if (background)
            {
                current.background = true;
            }
            break;
        }

        case TokenType::And:
        case TokenType::Or:
            error = ParseError{L"&& and || are not supported yet", token.position};
            return {};

        default:
            ++current_;
            break;
        }

        if (current_ >= tokens_.size())
        {
            break;
        }
    }

    if (!current.commands.empty())
    {
        pipelines.push_back(std::move(current));
    }
    return pipelines;
}

Command Parser::parseCommand(size_t& index, bool& background, std::optional<ParseError>& error)
{
    Command cmd;
    while (index < tokens_.size())
    {
        const Token& token = tokens_[index];

        switch (token.type)
        {
        case TokenType::End:
        case TokenType::Semicolon:
        case TokenType::Pipe:
        case TokenType::And:
        case TokenType::Or:
            return cmd;

        case TokenType::Background:
            background = true;
            ++index;
            return cmd;

        case TokenType::Word:
        case TokenType::Variable:
            if (cmd.program.empty())
            {
                cmd.program = token.value;
            }
            else
            {
                cmd.arguments.push_back(token.value);
            }
            ++index;
            break;

        case TokenType::Less:
        case TokenType::Greater:
        case TokenType::DoubleGreater:
        case TokenType::ErrorRedirect:
        case TokenType::ErrorOutput:
        case TokenType::ErrorAppend:
        case TokenType::BothOutput:
            if (!parseRedirection(index, cmd, error))
            {
                return cmd;
            }
            break;

        default:
            ++index;
            break;
        }
    }
    return cmd;
}

bool Parser::parseRedirection(size_t& index, Command& command, std::optional<ParseError>& error)
{
    const Token& token = tokens_[index];
    const RedirectType type = mapRedirectType(token.type);
    ++index;

    if (index >= tokens_.size() || tokens_[index].type == TokenType::End)
    {
        error = ParseError{L"Redirection operator requires a target", token.position};
        return false;
    }

    std::wstring target = parseRedirectTarget(index, error);
    if (error)
    {
        return false;
    }

    command.redirects.push_back(RedirectSpec{type, target});
    return true;
}

std::wstring Parser::parseRedirectTarget(size_t& index, std::optional<ParseError>& error)
{
    const Token& token = tokens_[index];

    if (token.type == TokenType::Word || token.type == TokenType::Variable)
    {
        ++index;
        return token.value;
    }

    if (token.type == TokenType::Less || token.type == TokenType::Greater ||
        token.type == TokenType::DoubleGreater || token.type == TokenType::ErrorRedirect ||
        token.type == TokenType::ErrorOutput || token.type == TokenType::ErrorAppend ||
        token.type == TokenType::BothOutput)
    {
        error = ParseError{L"Invalid redirection target", token.position};
        return L"";
    }

    error = ParseError{L"Expected a filename after redirection operator", token.position};
    return L"";
}

RedirectType Parser::mapRedirectType(TokenType type) const
{
    switch (type)
    {
    case TokenType::Less:           return RedirectType::Input;
    case TokenType::Greater:        return RedirectType::Output;
    case TokenType::DoubleGreater:  return RedirectType::AppendOutput;
    case TokenType::ErrorOutput:    return RedirectType::ErrorOutput;
    case TokenType::ErrorAppend:    return RedirectType::ErrorAppend;
    case TokenType::BothOutput:     return RedirectType::ErrorsToOutput;
    default:                        return RedirectType::Output;
    }
}

} // namespace kshell
