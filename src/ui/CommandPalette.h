#pragma once

#include "ui/Ui.h"
#include "ui/Key.h"
#include "ui/Fuzzy.h"
#include "render/ThemeManager.h"
#include "builtin/BuiltinRegistry.h"

#include <string>
#include <vector>
#include <functional>

namespace kshell::ui
{

struct PaletteEntry
{
    std::wstring  label;
    std::wstring  category;
    std::function<void()> action;
};

class CommandPalette
{
public:
    CommandPalette();

    // Show/hide.
    void open();
    void close();
    bool isOpen() const { return open_; }    // Draw the palette overlay on top of the current frame.
    void draw(RenderContext& rc);

    // Handle key events while the palette is open.
    // Returns true if consumed; if false, the key was Escape (palette closed).
    bool onKey(const KeyEvent& key);

    // Handle a click inside the palette overlay. rowInPane/colInPane are
    // relative to the overlay box. Returns true if consumed.
    bool onMouseClick(int rowInPane, int colInPane, bool doubleClick);

    // Set the action entries (command palette).
    void setEntries(std::vector<PaletteEntry> entries);

    // Theme switching entries are added dynamically from ThemeManager.
    void setThemeManager(const render::ThemeManager* tm);
    void setThemeSwitcher(std::function<void(const std::wstring&)> fn);

    // History search entries (set when Ctrl+R is invoked).
    void setHistoryEntries(const std::vector<std::wstring>& entries,
                           std::function<void(const std::wstring&)> onPick);

    // Called when a palette item is chosen but has no explicit action.
    // Lets the host insert the selected command into the shell input.
    void setOnInsert(std::function<void(const std::wstring&)> fn) { onInsert_ = std::move(fn); }

    const std::wstring& query() const { return query_; }
    int                 selectedIdx() const { return selectedIdx_; }

private:
    void buildDefaultEntries();
    void filterEntries();
    void rebuildEntries();

    bool                                    open_ = false;
    std::wstring                            query_;
    int                                     cursorPos_ = 0;
    int                                     selectedIdx_ = 0;
    std::vector<PaletteEntry>               allEntries_;
    std::vector<PaletteEntry>               filtered_;
    const render::ThemeManager*             themeMgr_ = nullptr;
    std::function<void(const std::wstring&)> themeSwitcher_;
    std::function<void(const std::wstring&)> historyPickFn_;
    std::function<void(const std::wstring&)> onInsert_;
    std::vector<std::wstring>              historyEntries_;
    bool                                    historyMode_ = false;
};

} // namespace kshell::ui
