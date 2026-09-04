#pragma once

#ifndef KSHELL_STRINGUTILS_H
#define KSHELL_STRINGUTILS_H

#include <string>
#include <vector>
#include <optional>

namespace kshell
{

namespace stringutils
{

std::wstring trim(const std::wstring& s);

std::wstring toLower(const std::wstring& s);

std::wstring toUpper(const std::wstring& s);

std::vector<std::wstring> split(const std::wstring& s, wchar_t delimiter);

bool startsWith(const std::wstring& s, const std::wstring& prefix);

bool endsWith(const std::wstring& s, const std::wstring& suffix);

bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b);

std::wstring replaceAll(const std::wstring& input, const std::wstring& from, const std::wstring& to);

std::string toUtf8(const std::wstring& wstr);

std::wstring fromUtf8(const std::string& str);

std::optional<std::wstring> fromNarrow(const std::string& str);

std::wstring join(const std::vector<std::wstring>& parts, const std::wstring& delimiter);

std::vector<std::wstring> splitCommandLine(const std::wstring& line);

} // namespace stringutils

} // namespace kshell

#endif // KSHELL_STRINGUTILS_H
