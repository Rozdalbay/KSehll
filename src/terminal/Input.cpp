#include "terminal/Input.h"

#include <algorithm>
#include <cwctype>

#include "utils/StringUtils.h"

namespace kshell
{

Input::Input(Console& console)
    : console_(console)
{
}

void Input::interruptRead()
{
    reading_ = false;
    cancelled_ = true;
}

InputResult Input::readLine(const std::wstring& prompt,
                            History& history,
                            Autocomplete& autocomplete,
                            const Config& config,
                            const std::wstring& workingDir,
                            const std::vector<std::wstring>& pathDirs,
                            const std::vector<std::wstring>& aliases,
                            const std::vector<std::wstring>& builtinNames)
{
    buffer_.clear();
    cursor_ = 0;
    prompt_ = prompt;
    history_ = &history;
    autocomplete_ = &autocomplete;
    config_ = &config;
    workingDir_ = workingDir;
    pathDirs_ = pathDirs;
    aliases_ = aliases;
    builtinNames_ = builtinNames;
    historyIndex_ = static_cast<int>(history.entries().size());
    savedLineForHistory_.clear();
    reading_ = true;
    cancelled_ = false;
    exitRequested_ = false;

    console_.print(prompt_);

    DWORD oldMode = 0;
    HANDLE hInput = console_.inputHandle();
    ::GetConsoleMode(hInput, &oldMode);
    DWORD newMode = oldMode;
    newMode &= ~ENABLE_PROCESSED_INPUT;
    newMode |= ENABLE_EXTENDED_FLAGS;
    ::SetConsoleMode(hInput, newMode);

    while (reading_)
    {
        INPUT_RECORD record = {};
        DWORD eventsRead = 0;
        if (!::ReadConsoleInputW(hInput, &record, 1, &eventsRead) || eventsRead == 0)
        {
            continue;
        }

        if (record.EventType != KEY_EVENT)
        {
            continue;
        }

        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        if (!key.bKeyDown)
        {
            continue;
        }

        const bool control = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        const bool shift = (key.dwControlKeyState & SHIFT_PRESSED) != 0;
        (void)shift;

        if (key.wVirtualKeyCode == VK_RETURN)
        {
            console_.print(L"\n");
            break;
        }
        else if (key.wVirtualKeyCode == VK_TAB)
        {
            handleTab();
            continue;
        }
        else if (key.wVirtualKeyCode == VK_ESCAPE)
        {
            console_.print(L"\n");
            cancelled_ = true;
            break;
        }
        else if (key.wVirtualKeyCode == VK_BACK)
        {
            if (cursor_ > 0)
            {
                deleteAtCursor();
            }
            continue;
        }
        else if (key.wVirtualKeyCode == VK_DELETE)
        {
            if (cursor_ < buffer_.size())
            {
                buffer_.erase(cursor_, 1);
                refresh();
            }
            continue;
        }
        else if (key.wVirtualKeyCode == VK_LEFT)
        {
            moveCursorLeft();
            continue;
        }
        else if (key.wVirtualKeyCode == VK_RIGHT)
        {
            moveCursorRight();
            continue;
        }
        else if (key.wVirtualKeyCode == VK_HOME)
        {
            moveToStart();
            continue;
        }
        else if (key.wVirtualKeyCode == VK_END)
        {
            moveToEnd();
            continue;
        }
        else if (key.wVirtualKeyCode == VK_UP)
        {
            if (historyIndex_ == -1)
            {
                savedLineForHistory_ = buffer_;
                historyIndex_ = static_cast<int>(history.entries().size());
            }
            if (historyIndex_ > 0 && !history.entries().empty())
            {
                --historyIndex_;
                buffer_ = history.entries()[static_cast<size_t>(historyIndex_)];
                cursor_ = buffer_.size();
                refresh();
            }
            continue;
        }
        else if (key.wVirtualKeyCode == VK_DOWN)
        {
            if (historyIndex_ == -1)
            {
                continue;
            }
            ++historyIndex_;
            if (historyIndex_ >= static_cast<int>(history.entries().size()))
            {
                historyIndex_ = static_cast<int>(history.entries().size());
                buffer_ = savedLineForHistory_;
            }
            else
            {
                buffer_ = history.entries()[static_cast<size_t>(historyIndex_)];
            }
            cursor_ = buffer_.size();
            refresh();
            continue;
        }
        else if (control && key.wVirtualKeyCode == L'C')
        {
            console_.print(L"\n");
            cancelled_ = true;
            break;
        }
        else if (control && key.wVirtualKeyCode == L'L')
        {
            console_.clear();
            console_.print(prompt_);
            refresh();
            continue;
        }
        else if (control && key.wVirtualKeyCode == L'D')
        {
            if (buffer_.empty())
            {
                console_.print(L"\n");
                exitRequested_ = true;
                break;
            }
            continue;
        }

        if (key.uChar.UnicodeChar != 0 && !control)
        {
            handleChar(key.uChar.UnicodeChar);
        }
    }

    ::SetConsoleMode(hInput, oldMode);
    reading_ = false;

    InputResult result;
    result.line = buffer_;
    result.cancelled = cancelled_;
    result.exitRequested = exitRequested_;
    return result;
}

void Input::handleChar(WCHAR ch)
{
    if (ch == L'\r' || ch == L'\n' || ch == L'\t')
    {
        return;
    }
    buffer_.insert(buffer_.begin() + static_cast<std::ptrdiff_t>(cursor_), ch);
    ++cursor_;
    console_.print(std::wstring(1, ch));
    if (cursor_ < buffer_.size())
    {
        refresh();
    }
}

void Input::refresh()
{
    console_.print(L"\r");
    console_.print(prompt_);
    console_.print(buffer_);

    const size_t padLen = buffer_.size();
    if (padLen > 0)
    {
        console_.print(std::wstring(padLen, L' '));
    }

    console_.print(L"\r");
    console_.print(prompt_);
    console_.print(buffer_.substr(0, cursor_));
}

void Input::printChar(WCHAR ch)
{
    console_.print(std::wstring(1, ch));
}

void Input::deleteAtCursor()
{
    buffer_.erase(cursor_ - 1, 1);
    --cursor_;
    refresh();
}

void Input::moveCursorLeft()
{
    if (cursor_ > 0)
    {
        --cursor_;
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        if (::GetConsoleScreenBufferInfo(console_.outputHandle(), &info))
        {
            if (info.dwCursorPosition.X > 0)
            {
                ::SetConsoleCursorPosition(console_.outputHandle(),
                                           COORD{SHORT(info.dwCursorPosition.X - 1),
                                                 info.dwCursorPosition.Y});
            }
        }
    }
}

void Input::moveCursorRight()
{
    if (cursor_ < buffer_.size())
    {
        ++cursor_;
        std::wstring nextChar = buffer_.substr(cursor_ - 1, 1);
        console_.print(nextChar);
    }
}

void Input::moveToStart()
{
    if (cursor_ == 0)
    {
        return;
    }
    SHORT width = 80;
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    if (::GetConsoleScreenBufferInfo(console_.outputHandle(), &info))
    {
        width = info.dwSize.X;
        COORD pos = info.dwCursorPosition;
        int steps = static_cast<int>(cursor_);
        int x = pos.X - steps;
        int y = pos.Y;
        while (x < 0)
        {
            x += width;
            --y;
        }
        ::SetConsoleCursorPosition(console_.outputHandle(), COORD{SHORT(x), SHORT(y)});
    }
    cursor_ = 0;
}

void Input::moveToEnd()
{
    while (cursor_ < buffer_.size())
    {
        moveCursorRight();
    }
}

void Input::clearLine()
{
    buffer_.clear();
    cursor_ = 0;
    std::wstring clear(prompt_.size() + 100, L' ');
    std::wstring back(prompt_.size() + 100, L'\b');
    console_.print(L"\r");
    console_.print(clear);
    console_.print(L"\r");
    console_.print(prompt_);
    (void)clear;
    (void)back;
}

void Input::handleTab()
{
    if (!config_ || !autocomplete_)
    {
        return;
    }
    const std::wstring prefix = currentWordPrefix();
    if (prefix.empty())
    {
        return;
    }

    std::vector<std::wstring> candidates;

    const size_t spacePos = buffer_.find_first_of(L" \t");
    const bool isFirstWord = (spacePos == std::wstring::npos || spacePos >= cursor_);

    if (isFirstWord)
    {
        candidates = autocomplete_->completeCommand(prefix, workingDir_, pathDirs_, aliases_);
    }
    else
    {
        candidates = autocomplete_->completeFileOrDir(prefix, workingDir_);
    }

    if (candidates.empty())
    {
        return;
    }

    if (candidates.size() == 1)
    {
        const std::wstring& candidate = candidates[0];
        const size_t wordStart = cursor_ - prefix.size();
        buffer_.replace(wordStart, prefix.size(), candidate);
        cursor_ = wordStart + candidate.size();
        refresh();
        return;
    }

    std::wstring common = candidates[0];
    for (size_t i = 1; i < candidates.size(); ++i)
    {
        size_t j = 0;
        while (j < common.size() && j < candidates[i].size() &&
               std::towlower(common[j]) == std::towlower(candidates[i][j]))
        {
            ++j;
        }
        common = common.substr(0, j);
    }

    if (common.size() > prefix.size())
    {
        const size_t wordStart = cursor_ - prefix.size();
        buffer_.replace(wordStart, prefix.size(), common);
        cursor_ = wordStart + common.size();
        refresh();
    }
    else
    {
        console_.print(L"\n");
        for (const auto& c : candidates)
        {
            console_.printLine(c);
        }
        console_.print(prompt_);
        refresh();
    }
}

std::wstring Input::currentWordPrefix()
{
    if (cursor_ > buffer_.size())
    {
        cursor_ = buffer_.size();
    }
    size_t start = cursor_;
    while (start > 0 && buffer_[start - 1] != L' ' && buffer_[start - 1] != L'\t')
    {
        --start;
    }
    return buffer_.substr(start, cursor_ - start);
}

void Input::appendToBuffer(const std::wstring& text)
{
    buffer_.insert(cursor_, text);
    cursor_ += text.size();
    refresh();
}

} // namespace kshell