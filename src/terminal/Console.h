#pragma once

#ifndef KSHELL_CONSOLE_H
#define KSHELL_CONSOLE_H

#include <windows.h>

#include <string>

#include "utils/WinHandle.h"
#include "config/Config.h"

namespace kshell
{

class Console
{
public:
    bool initialize();

    void setColor(unsigned short attributes);
    unsigned short getColor() const;
    void resetColor();

    void print(const std::wstring& text);
    void printLine(const std::wstring& text);
    void printPrompt(const std::wstring& prompt, const ColorSettings& colors);
    void printError(const std::wstring& text);
    void printOutput(const std::wstring& text, const ColorSettings& colors);

    void clear();

    HANDLE inputHandle() const;
    HANDLE outputHandle() const;
    bool isBuffered() const;

private:
    bool setConsoleModeInternal();

    WinHandle input_;
    WinHandle output_;
    unsigned short savedColor_ = 7;
    bool initialized_ = false;
};

} // namespace kshell

#endif // KSHELL_CONSOLE_H
