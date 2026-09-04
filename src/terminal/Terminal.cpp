#include "terminal/Terminal.h"

namespace kshell
{

Terminal::Terminal()
    : input_(console_)
{
}

bool Terminal::initialize()
{
    if (!console_.initialize())
    {
        return false;
    }
    return true;
}

void Terminal::setColor(unsigned short color)
{
    console_.setColor(color);
}

void Terminal::resetColor()
{
    console_.resetColor();
}

void Terminal::print(const std::wstring& text)
{
    console_.print(text);
}

void Terminal::printLine(const std::wstring& text)
{
    console_.printLine(text);
}

void Terminal::printPrompt()
{
    console_.print(context_.prompt);
}

void Terminal::printError(const std::wstring& text)
{
    console_.printError(text);
}

void Terminal::updateContext(const TerminalContext& context)
{
    context_ = context;
    workingDir_ = context.workingDir;
}

} // namespace kshell