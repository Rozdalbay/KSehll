#pragma once
#include <cstdint>

namespace kshell::render {

// An RGBA-ish color used by themes. The terminal layer maps these to the
// closest console palette entry or to true-color when available.
struct Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    bool    hasAlpha = false;

    constexpr Color() = default;
    constexpr Color(uint8_t red, uint8_t green, uint8_t blue)
        : r(red), g(green), b(blue) {}

    static Color rgb(uint8_t red, uint8_t green, uint8_t blue) { return Color(red, green, blue); }
    static Color transparent() { return Color(0, 0, 0); }

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

// Standard theme semantic roles. Components address colors by role so the
// actual values come from the active theme (Ctrl+Shift+P -> theme).
enum class Role : uint8_t
{
    Background,
    Foreground,
    Accent,
    AccentText,
    Selection,
    Border,
    StatusBar,
    StatusBarText,
    Sidebar,
    SidebarText,
    SidebarActive,
    Tab,
    TabActive,
    Success,
    Warning,
    Error,
    Muted,
    Highlight,
    CommandBar,
    CommandBarText,
    PanelTitle,
};

} // namespace kshell::render
