#pragma once

#ifndef KSHELL_TERMINAL_H
#define KSHELL_TERMINAL_H

#include <string>
#include <vector>

#include "terminal/Console.h"
#include "terminal/Input.h"
#include "history/History.h"
#include "autocomplete/Autocomplete.h"
#include "config/Config.h"

namespace kshell
{

struct TerminalContext
{
    std::wstring prompt;
    Config* config = nullptr;
    History* history = nullptr;
    Autocomplete* autocomplete = nullptr;
    std::wstring workingDir;
    std::vector<std::wstring> pathDirs;
    std::vector<std::wstring> aliases;
    std::vector<std::wstring> builtinNames;
};

class Terminal
{
public:
    Terminal();

    bool initialize();

    void setColor(unsigned short color);
    void resetColor();
    void print(const std::wstring& text);
    void printLine(const std::wstring& text);
    void printPrompt();
    void printError(const std::wstring& text);

    Console& console() { return console_; }
    Input& input() { return input_; }

    void updateContext(const TerminalContext& context);

    bool isExitRequested() const { return exitRequested_; }
    void requestExit(bool value) { exitRequested_ = value; }
    std::wstring currentLine() const { return lastLine_; }
    void setWorkingDir(const std::wstring& dir) { workingDir_ = dir; }

private:
    Console console_;
    Input input_;
    bool exitRequested_ = false;
    TerminalContext context_;
    std::wstring lastLine_;
    std::wstring workingDir_;
};

} // namespace kshell

#endif // KSHELL_TERMINAL_H