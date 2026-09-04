#include "terminal/Console.h"

namespace kshell
{

bool Console::initialize()
{
    input_.reset(::GetStdHandle(STD_INPUT_HANDLE));
    output_.reset(::GetStdHandle(STD_OUTPUT_HANDLE));
    if (!input_.valid() || !output_.valid())
    {
        return false;
    }
    initialized_ = true;
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    if (::GetConsoleScreenBufferInfo(output_.get(), &info))
    {
        savedColor_ = static_cast<unsigned short>(info.wAttributes);
    }
    return true;
}

bool Console::setConsoleModeInternal()
{
    if (!initialized_)
    {
        return false;
    }
    DWORD mode = 0;
    if (!::GetConsoleMode(input_.get(), &mode))
    {
        return false;
    }
    mode |= ENABLE_EXTENDED_FLAGS;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return ::SetConsoleMode(input_.get(), mode) != FALSE;
}

void Console::setColor(unsigned short attributes)
{
    if (output_.valid())
    {
        ::SetConsoleTextAttribute(output_.get(), attributes);
    }
}

unsigned short Console::getColor() const
{
    if (!output_.valid())
    {
        return 7;
    }
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    if (::GetConsoleScreenBufferInfo(output_.get(), &info))
    {
        return static_cast<unsigned short>(info.wAttributes & 0x00FF);
    }
    return 7;
}

void Console::resetColor()
{
    setColor(savedColor_);
}

void Console::print(const std::wstring& text)
{
    if (!output_.valid())
    {
        return;
    }
    DWORD written = 0;
    ::WriteConsoleW(output_.get(), text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
}

void Console::printLine(const std::wstring& text)
{
    print(text);
    print(L"\n");
}

void Console::printPrompt(const std::wstring& prompt, const ColorSettings& colors)
{
    setColor(colors.promptColor);
    print(prompt);
    resetColor();
}

void Console::printError(const std::wstring& text)
{
    setColor(savedColor_);
    print(text);
    print(L"\n");
    setColor(savedColor_);
}

void Console::printOutput(const std::wstring& text, const ColorSettings& colors)
{
    setColor(colors.outputColor);
    print(text);
    resetColor();
}

void Console::clear()
{
    if (!output_.valid())
    {
        return;
    }
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    if (!::GetConsoleScreenBufferInfo(output_.get(), &info))
    {
        return;
    }
    const COORD topLeft = {0, 0};
    DWORD cellCount = static_cast<DWORD>(info.dwSize.X * info.dwSize.Y);
    DWORD written = 0;
    ::FillConsoleOutputCharacterW(output_.get(), L' ', cellCount, topLeft, &written);
    ::FillConsoleOutputAttribute(output_.get(), savedColor_, cellCount, topLeft, &written);
    ::SetConsoleCursorPosition(output_.get(), topLeft);
}

HANDLE Console::inputHandle() const
{
    return input_.get();
}

HANDLE Console::outputHandle() const
{
    return output_.get();
}

} // namespace kshell
