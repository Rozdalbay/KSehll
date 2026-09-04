#pragma once

#include <string>

#include "config/Config.h"

namespace kshell
{

// Abstraction over "where shell output goes". The legacy REPL uses a console
// sink (writes to the real console). The TUI uses a buffer sink (writes into
// a session's scrollback). Keeping this an interface lets the same shell logic
// drive both front-ends without coupling to a particular output device.
class IOutputSink
{
public:
    virtual ~IOutputSink() = default;

    virtual void print(const std::wstring& text) = 0;
    virtual void printLine(const std::wstring& text) = 0;
    virtual void printPrompt(const std::wstring& prompt, const ColorSettings& colors) = 0;
    virtual void printError(const std::wstring& text) = 0;
    virtual void printSuccess(const std::wstring& text) = 0;
};

} // namespace kshell
