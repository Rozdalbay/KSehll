#include "utils/ErrorUtils.h"

#include <sstream>

namespace kshell
{
namespace errorutils
{

std::wstring getLastErrorMessage(DWORD errorCode)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = ::FormatMessageW(flags, nullptr, errorCode, 0,
                                          reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr)
    {
        std::wstringstream ss;
        ss << L"Unknown error (0x" << std::hex << errorCode << L")";
        return ss.str();
    }
    std::wstring message(buffer, length);
    ::LocalFree(buffer);

    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' ||
                                message.back() == L' ' || message.back() == L'.'))
    {
        message.pop_back();
    }
    return message;
}

std::wstring formatError(const wchar_t* context, DWORD errorCode)
{
    return std::wstring(context) + L": " + getLastErrorMessage(errorCode);
}

std::wstring friendlyErrorForPath(const std::wstring& path)
{
    DWORD error = ::GetLastError();
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return L"Error: path not found:\n" + path;
    case ERROR_ACCESS_DENIED:
        return L"Error: access denied:\n" + path;
    case ERROR_ALREADY_EXISTS:
        return L"Error: path already exists:\n" + path;
    default:
        return formatError(L"Error", error) + L"\n" + path;
    }
}

} // namespace errorutils
} // namespace kshell
