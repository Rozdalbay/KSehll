#include "render/Theme.h"

namespace kshell::render {

std::wstring Theme::toLines() const
{
    std::wstring out;
    out += L"name=" + name_ + L"\n";
    const wchar_t* names[] = {
        L"background", L"foreground", L"accent",      L"accent_text",
        L"selection",  L"border",     L"status_bar",  L"status_bar_text",
        L"sidebar",    L"sidebar_text", L"sidebar_active", L"tab",
        L"tab_active", L"success",    L"warning",     L"error",
        L"muted",      L"highlight",  L"command_bar", L"command_bar_text",
        L"panel_title",
    };
    auto rgbStr = [](Color c) {
        wchar_t buf[16];
        swprintf(buf, 16, L"%u,%u,%u", c.r, c.g, c.b);
        return std::wstring(buf);
    };
    const Role roles[] = {
        Role::Background,          Role::Foreground,      Role::Accent,
        Role::AccentText,          Role::Selection,       Role::Border,
        Role::StatusBar,           Role::StatusBarText,   Role::Sidebar,
        Role::SidebarText,         Role::SidebarActive,   Role::Tab,
        Role::TabActive,           Role::Success,         Role::Warning,
        Role::Error,               Role::Muted,           Role::Highlight,
        Role::CommandBar,          Role::CommandBarText,  Role::PanelTitle,
    };
    const size_t n = sizeof(roles) / sizeof(roles[0]);
    for (size_t i = 0; i < n && i < 21; ++i)
    {
        out += names[i];
        out += L"=";
        out += rgbStr(colors_[static_cast<size_t>(roles[i])]);
        out += L"\n";
    }
    return out;
}

bool Theme::fromLines(std::wstring_view lines)
{
    bool any = false;
    size_t start = 0;
    while (start <= lines.size())
    {
        size_t end = lines.find(L'\n', start);
        if (end == std::wstring_view::npos)
        {
            end = lines.size();
        }
        std::wstring_view line = lines.substr(start, end - start);
        start = (end == lines.size()) ? end + 1 : end + 1;

        // trim
        while (!line.empty() && (line.front() == L' ' || line.front() == L'\r'))
        {
            line.remove_prefix(1);
        }
        while (!line.empty() && (line.back() == L' ' || line.back() == L'\r'))
        {
            line.remove_suffix(1);
        }
        if (line.empty())
        {
            continue;
        }

        if (line.rfind(L"name=", 0) == 0)
        {
            name_ = std::wstring(line.substr(5));
            any = true;
            continue;
        }

        size_t eq = line.find(L'=');
        if (eq == std::wstring_view::npos)
        {
            continue;
        }
        std::wstring key(line.substr(0, eq));
        std::wstring val(line.substr(eq + 1));

        unsigned r, g, b;
        if (swscanf_s(val.c_str(), L"%u,%u,%u", &r, &g, &b) != 3)
        {
            continue;
        }
        if (r > 255 || g > 255 || b > 255)
        {
            continue;
        }
        Color c(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));

        if (key == L"background") { set(Role::Background, c); }
        else if (key == L"foreground") { set(Role::Foreground, c); }
        else if (key == L"accent") { set(Role::Accent, c); }
        else if (key == L"accent_text") { set(Role::AccentText, c); }
        else if (key == L"selection") { set(Role::Selection, c); }
        else if (key == L"border") { set(Role::Border, c); }
        else if (key == L"status_bar") { set(Role::StatusBar, c); }
        else if (key == L"status_bar_text") { set(Role::StatusBarText, c); }
        else if (key == L"sidebar") { set(Role::Sidebar, c); }
        else if (key == L"sidebar_text") { set(Role::SidebarText, c); }
        else if (key == L"sidebar_active") { set(Role::SidebarActive, c); }
        else if (key == L"tab") { set(Role::Tab, c); }
        else if (key == L"tab_active") { set(Role::TabActive, c); }
        else if (key == L"success") { set(Role::Success, c); }
        else if (key == L"warning") { set(Role::Warning, c); }
        else if (key == L"error") { set(Role::Error, c); }
        else if (key == L"muted") { set(Role::Muted, c); }
        else if (key == L"highlight") { set(Role::Highlight, c); }
        else if (key == L"command_bar") { set(Role::CommandBar, c); }
        else if (key == L"command_bar_text") { set(Role::CommandBarText, c); }
        else if (key == L"panel_title") { set(Role::PanelTitle, c); }

        any = true;
    }
    return any;
}

} // namespace kshell::render
