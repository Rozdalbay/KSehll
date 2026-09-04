#pragma once

#ifndef KSHELL_CONFIG_H
#define KSHELL_CONFIG_H

#include <string>
#include <vector>
#include <map>

namespace kshell
{

struct ColorSettings
{
    unsigned short promptColor = 11;
    unsigned short errorColor = 12;
    unsigned short successColor = 10;
    unsigned short outputColor = 7;
    unsigned short directoryColor = 14;
    unsigned short normalColor = 7;
};

class Config
{
public:
    Config();

    static constexpr int kDefaultHistorySize = 1000;

    bool load();
    bool save() const;
    void ensureDefaults();

    std::wstring getConfigDir() const;
    std::wstring getConfigFilePath() const;
    std::wstring getLogDir() const;
    std::wstring getLogFilePath() const;
    std::wstring getHistoryFilePath() const;

    std::wstring prompt = L"user@PC C:\\Users\\User>";
    int historySize = kDefaultHistorySize;
    bool autocompleteEnabled = true;
    bool colorEnabled = true;
    ColorSettings colors;
    std::vector<std::wstring> aliases;

    // TUI / environment settings (introduced with the KShell environment).
    std::wstring themeName = L"KShell Dark";
    std::wstring font = L"Consolas";
    int          fontSize = 0;            // 0 = keep current
    std::wstring cursorStyle = L"block";
    bool         animationsEnabled = false;
    int          sidebarWidth = 26;
    std::wstring defaultDir = L"";        // startup working dir

    void setAlias(const std::wstring& name, const std::wstring& value);
    bool removeAlias(const std::wstring& name);
    std::map<std::wstring, std::wstring> getAliases() const;

private:
    std::wstring configDir_;
    std::wstring configFilePath_;

    std::map<std::wstring, std::wstring> aliases_;
};

} // namespace kshell

#endif // KSHELL_CONFIG_H
