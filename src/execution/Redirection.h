#pragma once

#ifndef KSHELL_REDIRECTION_H
#define KSHELL_REDIRECTION_H

#include <windows.h>

#include <string>
#include <optional>

#include "utils/WinHandle.h"
#include "parser/Command.h"

namespace kshell
{

struct RedirectionPlan
{
    RedirectType type = RedirectType::Output;
    std::wstring target;
    WinHandle handle;
};

class Redirection
{
public:
    static std::optional<WinHandle> openInput(const std::wstring& path);

    static std::optional<WinHandle> openOutput(const std::wstring& path, bool append);

    static std::optional<WinHandle> openErrorOutput(const std::wstring& path, bool append);

    static std::optional<std::vector<RedirectionPlan>>
    prepare(const std::vector<RedirectSpec>& redirects);

private:
    static std::optional<WinHandle> createFileHandle(const std::wstring& path, DWORD access,
                                                     DWORD creationDisposition, DWORD flags);
};

} // namespace kshell

#endif // KSHELL_REDIRECTION_H
