#pragma once

#ifndef KSHELL_COMMAND_H
#define KSHELL_COMMAND_H

#include <string>
#include <vector>

namespace kshell
{

enum class RedirectType
{
    Input,
    Output,
    AppendOutput,
    ErrorOutput,
    ErrorAppend,
    ErrorsToOutput,
    BothOutput
};

struct RedirectSpec
{
    RedirectType type = RedirectType::Output;
    std::wstring target;
};

struct Command
{
    std::wstring program;
    std::vector<std::wstring> arguments;
    std::vector<RedirectSpec> redirects;
    bool background = false;
    bool useExpansion = true;

    std::wstring fullCommandLine() const
    {
        std::wstring result = program;
        for (const auto& arg : arguments)
        {
            result += L" ";
            result += arg;
        }
        return result;
    }
};

struct Pipeline
{
    std::vector<Command> commands;
    bool background = false;
};

} // namespace kshell

#endif // KSHELL_COMMAND_H
