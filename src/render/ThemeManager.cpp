#include "render/ThemeManager.h"

#include <algorithm>

namespace kshell::render {

ThemeManager::ThemeManager()
{
    registerBuiltins();
    active_ = themes_.front();
}

void ThemeManager::registerBuiltins()
{
    auto rgb = [](uint8_t r, uint8_t g, uint8_t b) { return Color::rgb(r, g, b); };

    // KShell Dark
    {
        Theme t(L"KShell Dark");
        t.setBackground(rgb(18, 21, 27));
        t.setForeground(rgb(222, 226, 234));
        t.set(Role::Accent, rgb(86, 138, 255));
        t.set(Role::AccentText, rgb(255, 255, 255));
        t.setSelection(rgb(56, 78, 132));
        t.setBorder(rgb(48, 54, 66));
        t.setStatusBar(rgb(28, 32, 40), rgb(178, 184, 196));
        t.setSidebar(rgb(24, 28, 36), rgb(168, 174, 185), rgb(86, 138, 255));
        t.setTab(rgb(28, 32, 40), rgb(86, 138, 255));
        t.setSuccess(rgb(64, 208, 128));
        t.setWarning(rgb(230, 176, 64));
        t.setError(rgb(240, 88, 96));
        t.setMuted(rgb(110, 117, 130));
        t.setHighlight(rgb(230, 200, 90));
        t.setCommandBar(rgb(28, 32, 40), rgb(222, 226, 234));
        t.setPanelTitle(rgb(86, 138, 255));
        themes_.push_back(std::move(t));
    }

    // KShell Light
    {
        Theme t(L"KShell Light");
        t.setBackground(rgb(248, 249, 252));
        t.setForeground(rgb(30, 34, 42));
        t.set(Role::Accent, rgb(38, 96, 222));
        t.set(Role::AccentText, rgb(255, 255, 255));
        t.setSelection(rgb(173, 200, 255));
        t.setBorder(rgb(210, 215, 224));
        t.setStatusBar(rgb(238, 240, 245), rgb(70, 76, 88));
        t.setSidebar(rgb(241, 243, 247), rgb(70, 76, 88), rgb(38, 96, 222));
        t.setTab(rgb(238, 240, 245), rgb(38, 96, 222));
        t.setSuccess(rgb(30, 140, 90));
        t.setWarning(rgb(190, 130, 20));
        t.setError(rgb(200, 40, 50));
        t.setMuted(rgb(130, 137, 148));
        t.setHighlight(rgb(160, 120, 0));
        t.setCommandBar(rgb(238, 240, 245), rgb(30, 34, 42));
        t.setPanelTitle(rgb(38, 96, 222));
        themes_.push_back(std::move(t));
    }

    // High Contrast
    {
        Theme t(L"High Contrast");
        t.setBackground(rgb(0, 0, 0));
        t.setForeground(rgb(255, 255, 255));
        t.set(Role::Accent, rgb(0, 190, 255));
        t.set(Role::AccentText, rgb(0, 0, 0));
        t.setSelection(rgb(0, 110, 160));
        t.setBorder(rgb(255, 255, 255));
        t.setStatusBar(rgb(0, 0, 0), rgb(0, 190, 255));
        t.setSidebar(rgb(0, 0, 0), rgb(255, 255, 255), rgb(0, 190, 255));
        t.setTab(rgb(0, 0, 0), rgb(0, 190, 255));
        t.setSuccess(rgb(0, 255, 120));
        t.setWarning(rgb(255, 220, 0));
        t.setError(rgb(255, 60, 60));
        t.setMuted(rgb(150, 150, 150));
        t.setHighlight(rgb(255, 255, 0));
        t.setCommandBar(rgb(0, 0, 0), rgb(255, 255, 255));
        t.setPanelTitle(rgb(0, 190, 255));
        themes_.push_back(std::move(t));
    }

    // Monochrome
    {
        Theme t(L"Monochrome");
        auto gray = [&](uint8_t v) { return rgb(v, v, v); };
        t.setBackground(gray(12));
        t.setForeground(gray(208));
        t.set(Role::Accent, gray(255));
        t.set(Role::AccentText, gray(20));
        t.setSelection(gray(90));
        t.setBorder(gray(56));
        t.setStatusBar(gray(28), gray(180));
        t.setSidebar(gray(16), gray(160), gray(255));
        t.setTab(gray(28), gray(220));
        t.setSuccess(gray(200));
        t.setWarning(gray(190));
        t.setError(gray(255));
        t.setMuted(gray(110));
        t.setHighlight(gray(255));
        t.setCommandBar(gray(28), gray(208));
        t.setPanelTitle(gray(255));
        themes_.push_back(std::move(t));
    }
}

int ThemeManager::activeIndex() const
{
    for (size_t i = 0; i < themes_.size(); ++i)
    {
        if (themes_[i].name() == active_.name())
        {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void ThemeManager::setActive(size_t index)
{
    if (index < themes_.size())
    {
        active_ = themes_[index];
    }
}

void ThemeManager::setActiveByName(const std::wstring& name)
{
    for (size_t i = 0; i < themes_.size(); ++i)
    {
        if (themes_[i].name() == name)
        {
            active_ = themes_[i];
            return;
        }
    }
    // Not found: replace active with a copy keeping the name so a bad config
    // never crashes and the palette still reflects it.
    active_.name(name);
}

bool ThemeManager::applyUserTheme(const std::wstring& name, std::wstring_view lines)
{
    Theme t(L"");
    if (!t.fromLines(lines))
    {
        return false;
    }
    if (!name.empty())
    {
        t.name(name);
    }
    // Replace matching name or append.
    for (auto& existing : themes_)
    {
        if (existing.name() == t.name())
        {
            existing = t;
            if (active_.name() == t.name())
            {
                active_ = t;
            }
            return true;
        }
    }
    themes_.push_back(t);
    return true;
}

void ThemeManager::setSelectionAndAccent(const Color& accent, const Color& selection)
{
    active_.set(Role::Accent, accent);
    active_.set(Role::AccentText, Color::rgb(255, 255, 255));
    active_.set(Role::Selection, selection);
}

} // namespace kshell::render
