#include "utils/PathUtils.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "utils/StringUtils.h"

namespace fs = std::filesystem;

namespace kshell
{
namespace pathutils
{

std::wstring getHomeDirectory()
{
    wchar_t buffer[MAX_PATH] = {0};
    DWORD size = ::GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH);
    if (size > 0 && size < MAX_PATH)
    {
        return std::wstring(buffer);
    }
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buffer)))
    {
        return std::wstring(buffer);
    }
    return L"C:\\";
}

std::wstring getAppDataDirectory()
{
    wchar_t buffer[MAX_PATH] = {0};
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer)))
    {
        return std::wstring(buffer);
    }
    return getHomeDirectory();
}

std::wstring getCurrentDirectory()
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

std::wstring expandPath(const std::wstring& path)
{
    if (path.empty())
    {
        return path;
    }

    std::wstring expanded = path;

    if (expanded[0] == L'~')
    {
        const std::wstring home = getHomeDirectory();
        if (expanded.size() == 1)
        {
            return home;
        }
        if (expanded[1] == L'\\' || expanded[1] == L'/')
        {
            expanded = home + expanded.substr(1);
        }
    }

    if (expanded.find(L"%") != std::wstring::npos)
    {
        DWORD size = ::ExpandEnvironmentStringsW(expanded.c_str(), nullptr, 0);
        if (size > 0)
        {
            std::wstring buffer(static_cast<size_t>(size) - 1, L'\0');
            ::ExpandEnvironmentStringsW(expanded.c_str(), buffer.data(), size);
            expanded = buffer;
        }
    }

    if (expanded.empty())
    {
        return expanded;
    }
    if (expanded[0] == L'.' && expanded.size() == 1)
    {
        return getCurrentDirectory();
    }
    if (expanded[0] == L'.' && expanded.size() >= 2 &&
        (expanded[1] == L'\\' || expanded[1] == L'/'))
    {
        expanded = getCurrentDirectory() + L"\\" + expanded.substr(2);
    }

    return expanded;
}

std::optional<std::wstring> searchExecutable(const std::wstring& name,
                                             const std::vector<std::wstring>& searchDirs)
{
    std::wstring ext = getExecutableExtension(name);
    const bool hasExt = !ext.empty();

    std::vector<std::wstring> extensions{
        L".exe", L".com", L".bat", L".cmd", L".LNK", L".ps1", L".vbs", L".js"
    };

    const std::wstring nameOnly = hasExt ? name.substr(0, name.size() - ext.size()) : name;
    const std::wstring baseLen = name;

    std::vector<std::wstring> candidates;
    if (hasExt)
    {
        candidates.push_back(name);
    }
    else
    {
        for (const auto& e : extensions)
        {
            candidates.push_back(name + e);
        }
    }

    for (const auto& dir : searchDirs)
    {
        for (const auto& candidate : candidates)
        {
            std::wstring full = dir;
            if (!full.empty() && full.back() != L'\\' && full.back() != L'/')
            {
                full += L"\\";
            }
            full += candidate;
            if (isExecutableFile(full))
            {
                return full;
            }
        }
    }

    return std::nullopt;
}

std::wstring getExecutableExtension(const std::wstring& name)
{
    const auto dot = name.rfind(L'.');
    if (dot == std::wstring::npos)
    {
        return L"";
    }
    const auto slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > dot)
    {
        return L"";
    }
    return name.substr(dot);
}

std::vector<std::wstring> getPathDirectories(const std::wstring& pathEnv)
{
    std::vector<std::wstring> dirs;
    const std::wstring trimmed = pathEnv;
    std::wstring current;
    for (wchar_t c : trimmed)
    {
        if (c == L';' || c == L':')
        {
            if (!current.empty())
            {
                dirs.push_back(current);
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
        dirs.push_back(current);
    }
    return dirs;
}

std::wstring normalizePath(const std::wstring& path)
{
    wchar_t buffer[MAX_PATH] = {0};
    DWORD result = ::GetFullPathNameW(path.c_str(), MAX_PATH, buffer, nullptr);
    if (result > 0 && result < MAX_PATH)
    {
        return std::wstring(buffer);
    }
    return path;
}

std::wstring getFileNameFromPath(const std::wstring& path)
{
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return path;
    }
    return path.substr(pos + 1);
}

std::wstring getParentDirectory(const std::wstring& path)
{
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return L"";
    }
    if (pos == 0)
    {
        return L"\\";
    }
    return path.substr(0, pos);
}

bool pathExists(const std::wstring& path)
{
    return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(const std::wstring& path)
{
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool isExecutableFile(const std::wstring& path)
{
    if (!pathExists(path) || isDirectory(path))
    {
        return false;
    }
    std::error_code ec;
    const auto size = fs::file_size(fs::path(path), ec);
    if (ec || size == 0)
    {
        return false;
    }
    const std::wstring lower = stringutils::toLower(path);
    return stringutils::endsWith(lower, L".exe") ||
           stringutils::endsWith(lower, L".com") ||
           stringutils::endsWith(lower, L".bat") ||
           stringutils::endsWith(lower, L".cmd");
}

std::vector<std::wstring> listDirectory(const std::wstring& directory)
{
    std::vector<std::wstring> entries;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::path(directory), ec))
    {
        if (!ec)
        {
            entries.push_back(entry.path().filename().wstring());
        }
    }
    return entries;
}

std::filesystem::path toFsPath(const std::wstring& path)
{
    return fs::path(path);
}

bool isAbsolutePath(const std::wstring& path)
{
    return fs::path(path).is_absolute();
}

} // namespace pathutils
} // namespace kshell
