#include "autocomplete/Autocomplete.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

namespace fs = std::filesystem;

namespace kshell
{

void Autocomplete::setBuiltinNames(const std::vector<std::wstring>& names)
{
    builtinNames_ = names;
}

std::vector<std::wstring> Autocomplete::completeFileOrDir(const std::wstring& prefix,
                                                          const std::wstring& workingDir) const
{
    std::vector<std::wstring> results;
    if (prefix.empty())
    {
        return results;
    }

    std::error_code ec;

    std::wstring dirPart;
    std::wstring filePart = prefix;

    const auto lastSlash = prefix.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
    {
        dirPart = prefix.substr(0, lastSlash + 1);
        filePart = prefix.substr(lastSlash + 1);
    }

    std::wstring fullDir = pathutils::expandPath(dirPart.empty() ? workingDir : dirPart);

    if (!fs::exists(fs::path(fullDir), ec) || !fs::is_directory(fs::path(fullDir), ec))
    {
        return results;
    }

    const std::wstring lowerPrefix = stringutils::toLower(filePart);
    for (const auto& entry : fs::directory_iterator(fs::path(fullDir), ec))
    {
        const std::wstring name = entry.path().filename().wstring();
        const std::wstring lowerName = stringutils::toLower(name);
        if (lowerName.rfind(lowerPrefix, 0) == 0)
        {
            std::wstring result = name;
            if (fs::is_directory(entry.path(), ec))
            {
                result += L"\\";
            }
            results.push_back(dirPart + result);
        }
    }

    std::sort(results.begin(), results.end());
    return results;
}

std::vector<std::wstring> Autocomplete::completeCommand(const std::wstring& prefix,
                                                        const std::wstring& workingDir,
                                                        const std::vector<std::wstring>& pathDirs,
                                                        const std::vector<std::wstring>& aliases) const
{
    std::vector<std::wstring> results;
    const std::wstring lowerPrefix = stringutils::toLower(prefix);

    for (const auto& name : builtinNames_)
    {
        if (stringutils::toLower(name).rfind(lowerPrefix, 0) == 0)
        {
            results.push_back(name);
        }
    }
    for (const auto& alias : aliases)
    {
        if (stringutils::toLower(alias).rfind(lowerPrefix, 0) == 0)
        {
            results.push_back(alias);
        }
    }

    for (const auto& dir : pathDirs)
    {
        std::error_code ec;
        const fs::path dirPath(dir);
        if (!fs::exists(dirPath, ec))
        {
            continue;
        }
        for (const auto& entry : fs::directory_iterator(dirPath, ec))
        {
            if (entry.is_directory(ec))
            {
                continue;
            }
            const std::wstring name = entry.path().filename().wstring();
            const std::wstring lowerName = stringutils::toLower(name);
            if (lowerName.rfind(lowerPrefix, 0) == 0 &&
                (stringutils::endsWith(lowerName, L".exe") ||
                 stringutils::endsWith(lowerName, L".com") ||
                 stringutils::endsWith(lowerName, L".bat") ||
                 stringutils::endsWith(lowerName, L".cmd")))
            {
                std::wstring stripped = name.substr(0, name.size() - 4);
                results.push_back(stripped);
            }
        }
    }

    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}

} // namespace kshell