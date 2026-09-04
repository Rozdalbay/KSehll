#pragma once

#include "terminal/IOutputSink.h"
#include "terminal/Console.h"

namespace kshell
{

// Adapts the legacy Console (direct console writes) to IOutputSink. Used by
// ShellContext by default so the classic REPL behaves exactly as before.
class ConsoleSink : public IOutputSink
{
public:
    explicit ConsoleSink(Console& console) : console_(console) {}

    void print(const std::wstring& text) override { console_.print(text); }
    void printLine(const std::wstring& text) override { console_.printLine(text); }
    void printPrompt(const std::wstring& prompt, const ColorSettings& colors) override
    {
        console_.printPrompt(prompt, colors);
    }
    void printError(const std::wstring& text) override { console_.printError(text); }
    void printSuccess(const std::wstring& text) override { console_.printLine(text); }

private:
    Console& console_;
};

} // namespace kshell
