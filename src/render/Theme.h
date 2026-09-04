#pragma once
#include "render/Color.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace kshell::render {

// A theme is a named palette mapping semantic Role -> Color. Colors live in
// themes only; components never hard-code colors.
class Theme
{
public:
    explicit Theme(std::wstring name) : name_(std::move(name)) {}

    const std::wstring& name() const { return name_; }
    void                name(std::wstring n) { name_ = std::move(n); }

    Color color(Role role) const
    {
        const auto idx = static_cast<size_t>(role);
        if (idx < colors_.size())
        {
            return colors_[idx];
        }
        return Color::rgb(0, 0, 0);
    }

    void set(Role role, Color c) { colors_[static_cast<size_t>(role)] = c; }

    // Standard role sets used by the built-in themes and by theme authors.
    void setBackground(Color c) { set(Role::Background, c); }
    void setForeground(Color c) { set(Role::Foreground, c); }
    void setAccent(Color c)
    {
        set(Role::Accent, c);
        set(Role::AccentText, {255, 255, 255});
    }
    void setSelection(Color c) { set(Role::Selection, c); }
    void setBorder(Color c) { set(Role::Border, c); }
    void setStatusBar(Color bg, Color fg)
    {
        set(Role::StatusBar, bg);
        set(Role::StatusBarText, fg);
    }
    void setSidebar(Color bg, Color fg, Color active)
    {
        set(Role::Sidebar, bg);
        set(Role::SidebarText, fg);
        set(Role::SidebarActive, active);
    }
    void setTab(Color bg, Color fg)
    {
        set(Role::Tab, bg);
        set(Role::TabActive, fg);
    }
    void setSuccess(Color c) { set(Role::Success, c); }
    void setWarning(Color c) { set(Role::Warning, c); }
    void setError(Color c) { set(Role::Error, c); }
    void setMuted(Color c) { set(Role::Muted, c); }
    void setHighlight(Color c) { set(Role::Highlight, c); }
    void setCommandBar(Color bg, Color fg)
    {
        set(Role::CommandBar, bg);
        set(Role::CommandBarText, fg);
    }
    void setPanelTitle(Color c) { set(Role::PanelTitle, c); }

    // Serialize to a simple lines format for user configurable themes.
    std::wstring toLines() const;
    bool         fromLines(std::wstring_view lines);

private:
    std::wstring            name_;
    std::array<Color, 24>   colors_{};
};

} // namespace kshell::render
