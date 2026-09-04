#include "environment/Environment.h"

#include <windows.h>

#include <cwctype>
#include <sstream>

namespace kshell
{

static std::wstring getEnvString(const wchar_t* name, const std::wstring& fallback)
{
    wchar_t buffer[32768] = {0};
    const DWORD size = ::GetEnvironmentVariableW(name, buffer, 32768);
    if (size > 0 && size < 32768)
    {
        return std::wstring(buffer);
    }
    (void)size;
    return fallback;
}

Environment::Environment()
{
    refresh();
}

std::optional<std::wstring> Environment::get(const std::wstring& name) const
{
    const auto it = variables_.find(name);
    if (it != variables_.end())
    {
        return it->second;
    }

    wchar_t buffer[32768] = {0};
    const DWORD size = ::GetEnvironmentVariableW(name.c_str(), buffer, 32768);
    if (size > 0 && size < 32768)
    {
        return std::wstring(buffer);
    }
    return std::nullopt;
}

void Environment::set(const std::wstring& name, const std::wstring& value)
{
    variables_[name] = value;
    userSet_.insert(name);
    ::SetEnvironmentVariableW(name.c_str(), value.c_str());
}

void Environment::unset(const std::wstring& name)
{
    variables_.erase(name);
    userSet_.erase(name);
    ::SetEnvironmentVariableW(name.c_str(), nullptr);
}

std::map<std::wstring, std::wstring> Environment::getAll() const
{
    std::map<std::wstring, std::wstring> result;
    LPWCH envStrings = ::GetEnvironmentStringsW();
    if (envStrings == nullptr)
    {
        return result;
    }

    for (LPWCH current = envStrings; *current != L'\0';)
    {
        const std::wstring entry(current);
        const auto eq = entry.find(L'=');
        if (eq != std::wstring::npos)
        {
            result[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
        current += static_cast<size_t>(entry.size()) + 1;
    }
    ::FreeEnvironmentStringsW(envStrings);

    for (const auto& [key, value] : variables_)
    {
        result[key] = value;
    }
    return result;
}

std::wstring Environment::getPath() const
{
    return getEnvString(L"PATH", L"");
}

std::wstring Environment::getHome() const
{
    return getEnvString(L"USERPROFILE", L"C:\\");
}

std::wstring Environment::getUser() const
{
    return getEnvString(L"USERNAME", L"user");
}

std::wstring Environment::getHostname() const
{
    return getEnvString(L"COMPUTERNAME", L"PC");
}

std::wstring Environment::getPwd() const
{
    DWORD size = ::GetCurrentDirectoryW(0, nullptr);
    if (size == 0)
    {
        return L"C:\\";
    }
    std::wstring result(static_cast<size_t>(size) - 1, L'\0');
    ::GetCurrentDirectoryW(size, result.data());
    return result;
}

void Environment::refresh()
{
    variables_ = getAll();
    variables_[L"USER"] = getUser();
    variables_[L"HOME"] = getHome();
    variables_[L"PWD"] = getPwd();
}

std::wstring Environment::expand(const std::wstring& input) const
{
    std::wstring result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size())
    {
        if (input[i] == L'%')
        {
            const size_t end = input.find(L'%', i + 1);
            if (end != std::wstring::npos)
            {
                const std::wstring name = input.substr(i + 1, end - i - 1);
                const auto it = variables_.find(name);
                if (it != variables_.end())
                {
                    result += it->second;
                }
                else if (auto value = get(name))
                {
                    result += *value;
                }
                else
                {
                    result += input.substr(i, end - i + 1);
                }
                i = end + 1;
                continue;
            }
        }
        if (input[i] == L'$')
        {
            const wchar_t next = (i + 1 < input.size()) ? input[i + 1] : L'\0';
            if (next == L'(')
            {
                result.push_back(input[i]);
                ++i;
                continue;
            }
            if (next == L' ' || next == L'\t' || next == L'\0')
            {
                result.push_back(input[i]);
                ++i;
                continue;
            }
            size_t j = i + 1;
            while (j < input.size() &&
                   (std::iswalnum(input[j]) || input[j] == L'_'))
            {
                ++j;
            }
            const std::wstring name = input.substr(i + 1, j - i - 1);
            const auto it = variables_.find(name);
            if (it != variables_.end())
            {
                result += it->second;
            }
            else if (auto value = get(name))
            {
                result += *value;
            }
            else
            {
                result += input.substr(i, j - i);
            }
            i = j;
            continue;
        }
        result.push_back(input[i]);
        ++i;
    }
    return result;
}

} // namespace kshell
