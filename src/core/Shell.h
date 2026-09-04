#pragma once

#ifndef KSHELL_SHELL_H
#define KSHELL_SHELL_H

#include <string>
#include <vector>

#include "core/ShellContext.h"
#include "terminal/Terminal.h"
#include "terminal/Input.h"
#include "autocomplete/Autocomplete.h"

namespace kshell
{

class Shell
{
public:
    Shell();
    ~Shell();

    bool initialize();
    int run();
    void shutdown();

private:
    enum class ProcessInputResult
    {
        Continue,
        Exit,
        Interrupted
    };

    ProcessInputResult processLine(const std::wstring& line);

    std::vector<std::wstring> resolveAliases(const std::vector<std::wstring>& tokens);

    bool expandVariables(std::wstring& input);

    void printBanner();

    ShellContext ctx_;
    Terminal terminal_;
    Autocomplete autocomplete_;
};

} // namespace kshell

#endif // KSHELL_SHELL_H