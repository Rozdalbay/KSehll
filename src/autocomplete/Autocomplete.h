#pragma once

#ifndef KSHELL_AUTOCOMPLETE_H
#define KSHELL_AUTOCOMPLETE_H

#include <string>
#include <vector>
#include <set>

namespace kshell
{

class Autocomplete
{
public:
    void setBuiltinNames(const std::vector<std::wstring>& names);

    struct CompletionResult
    {
        bool found = false;
        std::wstring completedText;
        std::vector<std::wstring> candidates;
        bool ambiguous = false;
    };

    std::vector<std::wstring> completeFileOrDir(const std::wstring& prefix,
                                                const std::wstring& workingDir) const;

    std::vector<std::wstring> completeCommand(const std::wstring& prefix,
                                              const std::wstring& workingDir,
                                              const std::vector<std::wstring>& pathDirs,
                                              const std::vector<std::wstring>& aliases) const;

private:
    std::vector<std::wstring> builtinNames_;
};

} // namespace kshell

#endif // KSHELL_AUTOCOMPLETE_H
