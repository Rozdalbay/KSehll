#pragma once

#ifndef KSHELL_INPUT_H
#define KSHELL_INPUT_H

#include <windows.h>

#include <vector>

#include "terminal/Console.h"
#include "history/History.h"
#include "autocomplete/Autocomplete.h"
#include "config/Config.h"

namespace kshell
{

struct InputResult
{
    std::wstring line;
    bool cancelled = false;
    bool exitRequested = false;
};

class Input
{
public:
    explicit Input(Console& console);

    InputResult readLine(const std::wstring& prompt,
                         History& history,
                         Autocomplete& autocomplete,
                         const Config& config,
                         const std::wstring& workingDir,
                         const std::vector<std::wstring>& pathDirs,
                         const std::vector<std::wstring>& aliases,
                         const std::vector<std::wstring>& builtinNames);

    void interruptRead();

private:
    void handleChar(WCHAR ch);
    void refresh();
    void printChar(WCHAR ch);
    void deleteAtCursor();
    void moveCursorLeft();
    void moveCursorRight();
    void moveToStart();
    void moveToEnd();
    void clearLine();
    void handleTab();
    void appendToBuffer(const std::wstring& text);
    std::wstring currentWordPrefix();

    Console& console_;
    std::wstring buffer_;
    size_t cursor_ = 0;
    std::wstring prompt_;
    bool reading_ = false;
    bool cancelled_ = false;
    bool exitRequested_ = false;

    History* history_ = nullptr;
    Autocomplete* autocomplete_ = nullptr;
    const Config* config_ = nullptr;
    std::wstring workingDir_;
    std::vector<std::wstring> pathDirs_;
    std::vector<std::wstring> aliases_;
    std::vector<std::wstring> builtinNames_;
    int historyIndex_ = -1;
    std::wstring savedLineForHistory_;
};

} // namespace kshell

#endif // KSHELL_INPUT_H