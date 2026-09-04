#include "utils/StringUtils.h"

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <iomanip>

namespace kshell
{
namespace stringutils
{

std::wstring trim(const std::wstring& s)
{
    const size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos)
    {
        return L"";
    }
    const size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::wstring toLower(const std::wstring& s)
{
    std::wstring result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return result;
}

std::wstring toUpper(const std::wstring& s)
{
    std::wstring result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return result;
}

std::vector<std::wstring> split(const std::wstring& s, wchar_t delimiter)
{
    std::vector<std::wstring> parts;
    std::wstring current;
    for (wchar_t c : s)
    {
        if (c == delimiter)
        {
            if (!current.empty())
            {
                parts.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(c);
        }
    }
    if (!current.empty())
    {
        parts.push_back(current);
    }
    return parts;
}

bool startsWith(const std::wstring& s, const std::wstring& prefix)
{
    if (prefix.size() > s.size())
    {
        return false;
    }
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::wstring& s, const std::wstring& suffix)
{
    if (suffix.size() > s.size())
    {
        return false;
    }
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b)
{
    return toLower(a) == toLower(b);
}

std::wstring replaceAll(const std::wstring& input, const std::wstring& from, const std::wstring& to)
{
    if (from.empty())
    {
        return input;
    }
    std::wstring result;
    size_t pos = 0;
    while (pos < input.size())
    {
        const size_t found = input.find(from, pos);
        if (found == std::wstring::npos)
        {
            result.append(input, pos, std::wstring::npos);
            break;
        }
        result.append(input, pos, found - pos);
        result.append(to);
        pos = found + from.size();
    }
    return result;
}

std::string toUtf8(const std::wstring& wstr)
{
    if (wstr.empty())
    {
        return {};
    }
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return {};
    }
    std::string result(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                          result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring fromUtf8(const std::string& str)
{
    if (str.empty())
    {
        return {};
    }
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
                                           nullptr, 0);
    if (size <= 0)
    {
        return {};
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
                          result.data(), size);
    return result;
}

std::optional<std::wstring> fromNarrow(const std::string& str)
{
    if (str.empty())
    {
        return std::wstring();
    }
    const int size = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()),
                                           nullptr, 0);
    if (size <= 0)
    {
        return std::nullopt;
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()),
                          result.data(), size);
    return result;
}

std::wstring join(const std::vector<std::wstring>& parts, const std::wstring& delimiter)
{
    std::wstring result;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
        {
            result += delimiter;
        }
        result += parts[i];
    }
    return result;
}

std::vector<std::wstring> splitCommandLine(const std::wstring& line)
{
    std::vector<std::wstring> parts;
    std::wstring current;
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    bool escaped = false;

    for (wchar_t c : line)
    {
        if (escaped)
        {
            current.push_back(c);
            escaped = false;
            continue;
        }

        if (inSingleQuote)
        {
            if (c == L'\'')
            {
                inSingleQuote = false;
            }
            else
            {
                current.push_back(c);
            }
            continue;
        }

        if (inDoubleQuote)
        {
            if (c == L'\\')
            {
                escaped = true;
            }
            else if (c == L'"')
            {
                inDoubleQuote = false;
            }
            else
            {
                current.push_back(c);
            }
            continue;
        }

        switch (c)
        {
        case L' ':
        case L'\t':
            if (!current.empty())
            {
                parts.push_back(current);
                current.clear();
            }
            break;
        case L'"':
            inDoubleQuote = true;
            break;
        case L'\'':
            inSingleQuote = true;
            break;
        case L'\\':
            escaped = true;
            break;
        default:
            current.push_back(c);
            break;
        }
    }

    if (escaped && !current.empty())
    {
        current.pop_back();
    }

    if (!current.empty())
    {
        parts.push_back(current);
    }
    return parts;
}

} // namespace stringutils
} // namespace kshell
