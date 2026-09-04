#pragma once

#ifndef KSHELL_PATHUTILS_H
#define KSHELL_PATHUTILS_H

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace kshell
{

namespace pathutils
{

std::wstring getHomeDirectory();

std::wstring getAppDataDirectory();

std::wstring getCurrentDirectory();

std::wstring expandPath(const std::wstring& path);

std::optional<std::wstring> searchExecutable(const std::wstring& name,
                                             const std::vector<std::wstring>& searchDirs);

std::wstring getExecutableExtension(const std::wstring& name);

std::vector<std::wstring> getPathDirectories(const std::wstring& pathEnv);

std::wstring normalizePath(const std::wstring& path);

std::wstring getFileNameFromPath(const std::wstring& path);

std::wstring getParentDirectory(const std::wstring& path);

bool pathExists(const std::wstring& path);

bool isDirectory(const std::wstring& path);

bool isExecutableFile(const std::wstring& path);

std::vector<std::wstring> listDirectory(const std::wstring& directory);

std::filesystem::path toFsPath(const std::wstring& path);

bool isAbsolutePath(const std::wstring& path);

} // namespace pathutils

} // namespace kshell

#endif // KSHELL_PATHUTILS_H
