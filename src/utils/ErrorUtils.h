#pragma once

#ifndef KSHELL_ERROUTILS_H
#define KSHELL_ERROUTILS_H

#include <windows.h>
#include <string>

namespace kshell
{

namespace errorutils
{

std::wstring getLastErrorMessage(DWORD errorCode);

std::wstring formatError(const wchar_t* context, DWORD errorCode);

std::wstring friendlyErrorForPath(const std::wstring& path);

} // namespace errorutils

} // namespace kshell

#endif // KSHELL_ERROUTILS_H
