#include "execution/Redirection.h"

#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"

namespace kshell
{

std::optional<WinHandle> Redirection::createFileHandle(const std::wstring& path, DWORD access,
                                                       DWORD creationDisposition, DWORD flags)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE handle = ::CreateFileW(
        path.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &sa,
        creationDisposition,
        flags,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
    {
        return std::nullopt;
    }
    return WinHandle(handle);
}

std::optional<WinHandle> Redirection::openInput(const std::wstring& path)
{
    const std::wstring fullPath = pathutils::expandPath(path);
    return createFileHandle(fullPath, GENERIC_READ, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
}

std::optional<WinHandle> Redirection::openOutput(const std::wstring& path, bool append)
{
    const std::wstring fullPath = pathutils::expandPath(path);
    const DWORD disposition = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    auto handle = createFileHandle(fullPath, GENERIC_WRITE, disposition, FILE_ATTRIBUTE_NORMAL);
    if (!handle)
    {
        return std::nullopt;
    }
    if (append)
    {
        ::SetFilePointer(handle->get(), 0, nullptr, FILE_END);
    }
    return handle;
}

std::optional<WinHandle> Redirection::openErrorOutput(const std::wstring& path, bool append)
{
    const std::wstring fullPath = pathutils::expandPath(path);
    const DWORD disposition = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    auto handle = createFileHandle(fullPath, GENERIC_WRITE, disposition, FILE_ATTRIBUTE_NORMAL);
    if (!handle)
    {
        return std::nullopt;
    }
    if (append)
    {
        ::SetFilePointer(handle->get(), 0, nullptr, FILE_END);
    }
    return handle;
}

std::optional<std::vector<RedirectionPlan>>
Redirection::prepare(const std::vector<RedirectSpec>& redirects)
{
    std::vector<RedirectionPlan> plans;
    plans.reserve(redirects.size());

    for (const auto& redirect : redirects)
    {
        RedirectionPlan plan;
        plan.type = redirect.type;
        plan.target = redirect.target;

        switch (redirect.type)
        {
        case RedirectType::Input:
        {
            if (plans.size() > 0 && plans.back().type == RedirectType::Input)
            {
                continue; // last one wins, but keep prior
            }
            auto handle = openInput(redirect.target);
            if (!handle)
            {
                return std::nullopt;
            }
            plan.handle = std::move(*handle);
            break;
        }
        case RedirectType::Output:
        case RedirectType::AppendOutput:
        {
            auto handle = openOutput(redirect.target, redirect.type == RedirectType::AppendOutput);
            if (!handle)
            {
                return std::nullopt;
            }
            plan.handle = std::move(*handle);
            break;
        }
        case RedirectType::ErrorOutput:
        case RedirectType::ErrorAppend:
        {
            auto handle = openErrorOutput(redirect.target,
                                          redirect.type == RedirectType::ErrorAppend);
            if (!handle)
            {
                return std::nullopt;
            }
            plan.handle = std::move(*handle);
            break;
        }
        case RedirectType::ErrorsToOutput:
        case RedirectType::BothOutput:
            plan.type = redirect.type;
            plan.handle.reset(nullptr);
            break;
        }
        plans.push_back(std::move(plan));
    }

    return plans;
}

} // namespace kshell
