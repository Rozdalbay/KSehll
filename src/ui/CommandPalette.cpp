#include "ui\CommandPalette.h"
#include "core/Locale.h"

namespace kshell::ui
{

CommandPalette::CommandPalette()
{
    buildDefaultEntries();
}

void CommandPalette::buildDefaultEntries()
{
    allEntries_.clear();
    for (const auto& reg : builtinRegistry())
    {
        PaletteEntry e;
        e.label = reg.name;
        e.category = reg.help.category;
        // Selecting a builtin with no action inserts it into the shell's
        // input line (handled via onInsert_ callback).
        allEntries_.push_back(std::move(e));
    }

    // Add a settings separator and theme entries (filled when themeMgr_ set).
    rebuildEntries();
}

void CommandPalette::rebuildEntries()
{
    // Re-add the theme group if a manager is available; otherwise skip.
    if (!themeMgr_)
    {
        return;
    }

    // Remove any previous theme entries.
    for (auto it = allEntries_.begin(); it != allEntries_.end(); )
    {
        if (it->category == L"THEME")
        {
            it = allEntries_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (int i = 0; i < themeMgr_->count(); ++i)
    {
        std::wstring name = themeMgr_->nameAt(i);
        PaletteEntry e;
        e.label = name;
        e.category = L"THEME";
        std::wstring chosen = name;
        e.action = [this, chosen]() {
            if (themeSwitcher_)
            {
                themeSwitcher_(chosen);
            }
        };
        allEntries_.push_back(std::move(e));
    }
}

void CommandPalette::open()
{
    open_ = true;
    query_.clear();
    cursorPos_ = 0;
    selectedIdx_ = 0;
    historyMode_ = false;
    filterEntries();
}

void CommandPalette::close()
{
    open_ = false;
}

void CommandPalette::setEntries(std::vector<PaletteEntry> entries)
{
    allEntries_ = std::move(entries);
}

void CommandPalette::setThemeManager(const render::ThemeManager* tm)
{
    themeMgr_ = tm;
    rebuildEntries();
}

void CommandPalette::setThemeSwitcher(std::function<void(const std::wstring&)> fn)
{
    themeSwitcher_ = std::move(fn);
}

void CommandPalette::setHistoryEntries(const std::vector<std::wstring>& entries,
                                       std::function<void(const std::wstring&)> onPick)
{
    historyEntries_ = entries;
    historyPickFn_ = std::move(onPick);
    historyMode_ = true;
    open_ = true;
    query_.clear();
    cursorPos_ = 0;
    selectedIdx_ = 0;
    filterEntries();
}

void CommandPalette::filterEntries()
{
    filtered_.clear();

    if (historyMode_)
    {
        for (size_t i = 0; i < historyEntries_.size(); ++i)
        {
            auto m = fuzzyMatch(query_, historyEntries_[i]);
            if (m.matched || query_.empty())
            {
                PaletteEntry e;
                e.label = historyEntries_[i];
                e.category = L"History";
                filtered_.push_back(std::move(e));
            }
        }
        if (selectedIdx_ >= (int)filtered_.size())
        {
            selectedIdx_ = (int)filtered_.size() - 1;
        }
        if (selectedIdx_ < 0)
        {
            selectedIdx_ = 0;
        }
        return;
    }

    // Normal mode: filter all entries.
    if (query_.empty())
    {
        filtered_ = allEntries_;
    }
    else
    {
        for (const auto& e : allEntries_)
        {
            auto m = fuzzyMatch(query_, e.label);
            if (m.matched)
            {
                filtered_.push_back(e);
            }
        }
    }

    if (selectedIdx_ >= (int)filtered_.size())
    {
        selectedIdx_ = (int)filtered_.size() - 1;
    }
    if (selectedIdx_ < 0)
    {
        selectedIdx_ = 0;
    }
}

bool CommandPalette::onKey(const KeyEvent& key)
{
    if (!open_)
    {
        return false;
    }

    if (key.key == Key::Escape)
    {
        close();
        return false;
    }

    if (key.key == Key::Up || (key.ctrl && key.ch == L'k'))
    {
        if (selectedIdx_ > 0)
        {
            --selectedIdx_;
        }
        return true;
    }

    if (key.key == Key::Down || (key.ctrl && key.ch == L'j'))
    {
        if (selectedIdx_ < (int)filtered_.size() - 1)
        {
            ++selectedIdx_;
        }
        return true;
    }

    if (key.key == Key::Enter)
    {
        if (selectedIdx_ >= 0 && selectedIdx_ < (int)filtered_.size())
        {
            const auto& entry = filtered_[selectedIdx_];
            if (historyMode_)
            {
                if (historyPickFn_)
                {
                    historyPickFn_(entry.label);
                }
                close();
                return false;
            }
            if (entry.action)
            {
                entry.action();
                close();
                return false;
            }
            // No action: insert the selected command into the shell input.
            if (onInsert_)
            {
                onInsert_(entry.label);
                close();
                return false;
            }
        }
        close();
        return false;
    }

    if (key.key == Key::Backspace)
    {
        if (!query_.empty())
        {
            query_.pop_back();
            --cursorPos_;
        }
        filterEntries();
        return true;
    }

    if (key.isPrint())
    {
        query_.insert(query_.begin() + cursorPos_, key.ch);
        ++cursorPos_;
        filterEntries();
        return true;
    }

    return true;
}

bool CommandPalette::onMouseClick(int rowInPane, int colInPane, bool doubleClick)
{
    (void)rowInPane; (void)colInPane; (void)doubleClick;
    // Keep the palette open on click; clicks inside the palette overlay do not
    // fall through to the underlying panel.
    return true;
}

void CommandPalette::draw(RenderContext& rc)
{
    if (!open_)
    {
        return;
    }

    const auto& t = rc.theme;
    const render::Color bg = t.color(render::Role::CommandBar);
    const render::Color fg = t.color(render::Role::CommandBarText);
    const render::Color accent = t.color(render::Role::Accent);
    const render::Color selection = t.color(render::Role::Selection);

    // Full overlay: centered box.
    int boxW = std::min(rc.bounds.w - 4, 60);
    int boxH = std::min(rc.bounds.h - 4, 20);
    int boxX = rc.bounds.x + (rc.bounds.w - boxW) / 2;
    int boxY = rc.bounds.y + (rc.bounds.h - boxH) / 2;

    // Clear entire background.
    rc.screen.fillRect(rc.bounds.y, rc.bounds.x, rc.bounds.h, rc.bounds.w,
                       L' ', t.color(render::Role::Background), t.color(render::Role::Background));

    // Draw border.
    rc.screen.drawBorder(boxY, boxX, boxH, boxW, t.color(render::Role::Border), bg);

    // Draw title.
    std::wstring title = historyMode_ ? tr(L"Search History") : tr(L"Command Palette");
    rc.screen.putText(boxY + 1, boxX + 2, title, accent, bg, true);

    // Draw search input.
    rc.screen.fillLine(boxY + 2, boxX + 1, boxW - 2, L' ', bg, bg);
    std::wstring searchLine = L"> " + query_;
    rc.screen.putText(boxY + 2, boxX + 2, searchLine, fg, bg);
    // Draw cursor.
    int cursorX = boxX + 2 + (int)searchLine.size();
    if (cursorX < boxX + boxW - 1)
    {
        rc.screen.put(boxY + 2, cursorX, L'\u2588', accent, bg);
    }

    // Draw separator.
    rc.screen.fillLine(boxY + 3, boxX + 1, boxW - 2, L'\u2500', t.color(render::Role::Border), bg);

    // Draw entries.
    int maxVisible = boxH - 5;
    int scrollOffset = 0;
    if (selectedIdx_ >= maxVisible)
    {
        scrollOffset = selectedIdx_ - maxVisible + 1;
    }

    for (int i = 0; i < maxVisible; ++i)
    {
        int idx = i + scrollOffset;
        if (idx >= (int)filtered_.size())
        {
            break;
        }
        const auto& entry = filtered_[idx];
        bool isSelected = (idx == selectedIdx_);
        render::Color rowBg = isSelected ? selection : bg;

        std::wstring label = entry.label;
        if ((int)label.size() > boxW - 18)
        {
            label = label.substr(0, boxW - 21) + L"...";
        }

        std::wstring cat = entry.category;
        if ((int)cat.size() > 12)
        {
            cat = cat.substr(0, 12);
        }

        rc.screen.fillLine(boxY + 4 + i, boxX + 1, boxW - 2, L' ', rowBg, rowBg);
        rc.screen.putText(boxY + 4 + i, boxX + 2, label, isSelected ? accent : fg, rowBg);
        rc.screen.putText(boxY + 4 + i, boxX + boxW - 2 - (int)cat.size(), cat, t.color(render::Role::Muted), rowBg);
    }

    // Draw footer.
    std::wstring footer = std::to_wstring(filtered_.size()) + tr(L" items");
    rc.screen.putText(boxY + boxH - 1, boxX + 2, footer, t.color(render::Role::Muted), bg);
}

} // namespace kshell::ui
