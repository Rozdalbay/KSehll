#pragma once
#include "render/Theme.h"

namespace kshell::render {

// ThemeManager owns the built-in themes and the currently active one.
// Switching themes is a pure data operation; the renderer re-reads on next
// frame, so no hard-coded colors exist anywhere else.
class ThemeManager
{
public:
    ThemeManager();

    const Theme& activeTheme() const { return active_; }
    Theme&       activeThemeMut() { return active_; }

    // 0-based index of the active theme among themes_.
    int activeIndex() const;

    void setActive(size_t index);
    void setActiveByName(const std::wstring& name);

    size_t count() const { return themes_.size(); }
    const std::wstring& nameAt(size_t i) const { return themes_[i].name(); }

    // Apply a user theme from config (merges into themes_ or replaces active).
    bool applyUserTheme(const std::wstring& name, std::wstring_view lines);

    void setSelectionAndAccent(const Color& accent, const Color& selection);

private:
    void registerBuiltins();
    std::vector<Theme> themes_;
    Theme              active_{L"KShell Dark"};
};

} // namespace kshell::render
