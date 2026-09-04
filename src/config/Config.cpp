#include "config/Config.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <filesystem>
#include <sstream>

#include "utils/StringUtils.h"
#include "utils/PathUtils.h"

namespace fs = std::filesystem;

namespace kshell
{

Config::Config()
{
    std::wstring appData = pathutils::getAppDataDirectory();
    if (appData.empty())
    {
        appData = L"C:\\";
    }
    configDir_ = appData + L"\\KShell";
    configFilePath_ = configDir_ + L"\\config";
    ensureDefaults();
    load();
}

bool Config::load()
{
    std::wifstream file(configFilePath_.c_str());
    if (!file.is_open())
    {
        return false;
    }

    std::wstring line;
    while (std::getline(file, line))
    {
        line = stringutils::trim(line);
        if (line.empty() || line[0] == L'#')
        {
            continue;
        }

        const auto eq = line.find(L'=');
        if (eq == std::wstring::npos)
        {
            continue;
        }

        const std::wstring key = stringutils::trim(line.substr(0, eq));
        std::wstring value = stringutils::trim(line.substr(eq + 1));

        if (value.size() >= 2 &&
            ((value.front() == L'"' && value.back() == L'"') ||
             (value.front() == L'\'' && value.back() == L'\'')))
        {
            value = value.substr(1, value.size() - 2);
        }

        if (key == L"prompt")
        {
            prompt = value;
        }
        else if (key == L"history_size")
        {
            try
            {
                historySize = std::stoi(value);
                if (historySize <= 0)
                {
                    historySize = kDefaultHistorySize;
                }
            }
            catch (...)
            {
                historySize = kDefaultHistorySize;
            }
        }
        else if (key == L"autocomplete")
        {
            autocompleteEnabled = stringutils::equalsIgnoreCase(value, L"true") || value == L"1";
        }
        else if (key == L"color")
        {
            colorEnabled = stringutils::equalsIgnoreCase(value, L"true") || value == L"1";
        }
        else if (key == L"alias")
        {
            const auto space = value.find(L' ');
            if (space != std::wstring::npos)
            {
                const std::wstring name = stringutils::trim(value.substr(0, space));
                std::wstring cmd = stringutils::trim(value.substr(space + 1));
                if (cmd.size() >= 2 &&
                    ((cmd.front() == L'"' && cmd.back() == L'"') ||
                     (cmd.front() == L'\'' && cmd.back() == L'\'')))
                {
                    cmd = cmd.substr(1, cmd.size() - 2);
                }
                aliases_[name] = cmd;
            }
        }
        else if (key == L"prompt_color")
        {
            try { colors.promptColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"error_color")
        {
            try { colors.errorColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"success_color")
        {
            try { colors.successColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"output_color")
        {
            try { colors.outputColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"directory_color")
        {
            try { colors.directoryColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"normal_color")
        {
            try { colors.normalColor = static_cast<unsigned short>(std::stoi(value)); } catch (...) {}
        }
        else if (key == L"theme")
        {
            themeName = value;
        }
        else if (key == L"font")
        {
            font = value;
        }
        else if (key == L"font_size")
        {
            try { fontSize = std::stoi(value); } catch (...) {}
        }
        else if (key == L"cursor_style")
        {
            cursorStyle = value;
        }
        else if (key == L"animations")
        {
            animationsEnabled = stringutils::equalsIgnoreCase(value, L"true") || value == L"1";
        }
        else if (key == L"sidebar_width")
        {
            try { sidebarWidth = std::stoi(value); if (sidebarWidth < 12) sidebarWidth = 12; } catch (...) {}
        }
        else if (key == L"default_dir")
        {
            defaultDir = value;
        }
    }
    return true;
}

bool Config::save() const
{
    std::error_code ec;
    fs::create_directories(fs::path(configDir_), ec);
    if (ec)
    {
        return false;
    }

    std::wofstream file(configFilePath_.c_str(), std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }

    file << L"# KShell configuration\n";
    file << L"# Generated automatically. Edit with care.\n\n";
    file << L"prompt=" << prompt << L"\n";
    file << L"history_size=" << historySize << L"\n";
    file << L"autocomplete=" << (autocompleteEnabled ? L"true" : L"false") << L"\n";
    file << L"color=" << (colorEnabled ? L"true" : L"false") << L"\n";
    file << L"prompt_color=" << colors.promptColor << L"\n";
    file << L"error_color=" << colors.errorColor << L"\n";
    file << L"success_color=" << colors.successColor << L"\n";
    file << L"output_color=" << colors.outputColor << L"\n";
    file << L"directory_color=" << colors.directoryColor << L"\n";
    file << L"normal_color=" << colors.normalColor << L"\n";
    file << L"theme=" << themeName << L"\n";
    file << L"font=" << font << L"\n";
    file << L"font_size=" << fontSize << L"\n";
    file << L"cursor_style=" << cursorStyle << L"\n";
    file << L"animations=" << (animationsEnabled ? L"true" : L"false") << L"\n";
    file << L"sidebar_width=" << sidebarWidth << L"\n";
    if (!defaultDir.empty())
    {
        file << L"default_dir=" << defaultDir << L"\n";
    }

    for (const auto& [name, value] : aliases_)
    {
        file << L"alias=" << name << L" \"" << value << L"\"\n";
    }
    return true;
}

void Config::ensureDefaults()
{
    if (!std::filesystem::exists(fs::path(configFilePath_)))
    {
        save();
    }
}

std::wstring Config::getConfigDir() const
{
    return configDir_;
}

std::wstring Config::getConfigFilePath() const
{
    return configFilePath_;
}

std::wstring Config::getLogDir() const
{
    return configDir_ + L"\\logs";
}

std::wstring Config::getLogFilePath() const
{
    return getLogDir() + L"\\kshell.log";
}

std::wstring Config::getHistoryFilePath() const
{
    return pathutils::getHomeDirectory() + L"\\.kshell_history";
}

void Config::setAlias(const std::wstring& name, const std::wstring& value)
{
    aliases_[name] = value;
    save();
}

bool Config::removeAlias(const std::wstring& name)
{
    const auto it = aliases_.find(name);
    if (it == aliases_.end())
    {
        return false;
    }
    aliases_.erase(it);
    save();
    return true;
}

std::map<std::wstring, std::wstring> Config::getAliases() const
{
    return aliases_;
}

} // namespace kshell
