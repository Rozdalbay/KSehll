#include "ui/App.h"
#include "ui/Fuzzy.h"
#include "ui/ExecEngine.h"
#include "utils/PathUtils.h"
#include "jobs/JobManager.h"
#include "history/History.h"
#include "environment/Environment.h"
#include "core/Locale.h"

#include <windows.h>

#include <algorithm>
#include <memory>
#include <sstream>

namespace kshell::ui
{

namespace
{

const wchar_t* kViewNames[] = {
    L"Terminal", L"Files", L"Processes", L"System", L"Git",
    L"Jobs", L"History", L"Environment", L"Variables", L"Trace"
};

// Convert COLORREF (R,G,B) to ANSI 24-bit color escape.
std::wstring ansi24(render::Color c)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"\x1b[38;2;%u;%u;%um", c.r, c.g, c.b);
    return buf;
}

std::wstring ansi24bg(render::Color c)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"\x1b[48;2;%u;%u;%um", c.r, c.g, c.b);
    return buf;
}

std::wstring ansiReset()
{
    return L"\x1b[0m";
}

std::wstring ansiBold(bool on)
{
    return on ? L"\x1b[1m" : L"\x1b[22m";
}

std::wstring ansiMove(int row, int col)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"\x1b[%d;%dH", row, col);
    return buf;
}

} // namespace

App::App() = default;
App::~App() { shutdown(); }

bool App::initConsole()
{
    hIn_ = ::GetStdHandle(STD_INPUT_HANDLE);
    hOut_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn_ == INVALID_HANDLE_VALUE || hOut_ == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    ::GetConsoleMode(hIn_, &savedInMode_);
    ::GetConsoleMode(hOut_, &savedOutMode_);

    // Switch to alternate screen buffer.
    DWORD outMode = savedOutMode_;
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (::SetConsoleMode(hOut_, outMode))
    {
        vtProcessing_ = true;
    }

    // Activate alternate screen buffer.
    if (vtProcessing_)
    {
        std::wstring alt = L"\x1b[?1049h";
        DWORD written = 0;
        ::WriteConsoleW(hOut_, alt.c_str(), (DWORD)alt.size(), &written, nullptr);
        altScreen_ = true;
    }

    // Enable extended input.
    DWORD inMode = savedInMode_;
    inMode |= ENABLE_EXTENDED_FLAGS;
    inMode &= ~ENABLE_QUICK_EDIT_MODE;
    inMode |= ENABLE_MOUSE_INPUT;
    ::SetConsoleMode(hIn_, inMode);

    // Hide cursor (we draw our own).
    if (vtProcessing_)
    {
        std::wstring hide = L"\x1b[?25l";
        DWORD written = 0;
        ::WriteConsoleW(hOut_, hide.c_str(), (DWORD)hide.size(), &written, nullptr);
    }

    return true;
}

void App::restoreConsole()
{
    if (vtProcessing_)
    {
        // Show cursor and restore screen.
        std::wstring show = L"\x1b[?25h\x1b[?1049l";
        DWORD written = 0;
        ::WriteConsoleW(hOut_, show.c_str(), (DWORD)show.size(), &written, nullptr);
    }
    ::SetConsoleMode(hIn_, savedInMode_);
    ::SetConsoleMode(hOut_, savedOutMode_);
}

bool App::initialize(int argc, wchar_t* argv[])
{
    config_.load();
    themeMgr_.setActiveByName(config_.themeName);

    if (!initConsole())
    {
        return false;
    }

    // Get initial terminal size.
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (::GetConsoleScreenBufferInfo(hOut_, &csbi))
    {
        termWidth_ = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        termHeight_ = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    else
    {
        termWidth_ = 80;
        termHeight_ = 25;
    }
    screen_.resize(termHeight_, termWidth_);

    // Create first tab.
    auto pane = std::make_unique<ShellPane>(L"terminal-1");
    std::wstring startupDir = config_.defaultDir.empty()
                                  ? pathutils::getCurrentDirectory()
                                  : config_.defaultDir;
    pane->initialize(startupDir);
    tabs_.push_back(std::move(pane));
    activeTab_ = 0;

    // Set up sidebar panels with context from the active shell.
    filePane_.navigateTo(startupDir);
    gitPane_.setWorkDir(startupDir);

    running_ = true;
    return true;
}

int App::run()
{
    while (running_)
    {
        // Poll for input with a timeout so we can refresh periodically.
        DWORD wait = ::WaitForSingleObject(hIn_, 50);

        if (wait == WAIT_OBJECT_0)
        {
            INPUT_RECORD recs[32];
            DWORD read = 0;
            if (::ReadConsoleInputW(hIn_, recs, 32, &read) && read > 0)
            {
                for (DWORD i = 0; i < read; ++i)
                {
                    const auto& rec = recs[i];
                    if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
                    {
                        KeyEvent key = translateInput(rec);

                        // Global keys first.
                        handleGlobalKeys(key);

                        // Route to active palette, active panel, or terminal tab.
                        if (palette_.isOpen())
                        {
                            palette_.onKey(key);
                        }
                        else
                        {
                            switch (viewMode_)
                            {
                            case ViewMode::Terminal:
                                if (activeTab_ < (int)tabs_.size())
                                {
                                    tabs_[activeTab_]->onKey(key);
                                }
                                break;
                            case ViewMode::Files:      filePane_.onKey(key); break;
                            case ViewMode::Processes:  processPane_.onKey(key); break;
                            case ViewMode::System:     systemPane_.onKey(key); break;
                            case ViewMode::Git:        gitPane_.onKey(key); break;
                            case ViewMode::Jobs:       jobsPane_.onKey(key); break;
                            case ViewMode::History:    historyPane_.onKey(key); break;
                            case ViewMode::Environment: envPane_.onKey(key); break;
                            case ViewMode::Variables:  variablesPane_.onKey(key); break;
                            case ViewMode::Trace:      tracePane_.onKey(key); break;
                            }
                        }
                    }
                    else if (rec.EventType == MOUSE_EVENT)
                    {
                        handleMouseEvent(rec.Event.MouseEvent);
                    }
                    else if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT)
                    {
                        termWidth_ = rec.Event.WindowBufferSizeEvent.dwSize.X;
                        termHeight_ = rec.Event.WindowBufferSizeEvent.dwSize.Y;
                        screen_.resize(termHeight_, termWidth_);
                    }
                }
            }
        }

        // Periodic refresh of panels.
        static ULONGLONG lastRefreshTime = 0;
        ULONGLONG now = ::GetTickCount64();
        if (now - lastRefreshTime >= 1000)
        {
            lastRefreshTime = now;
            if (viewMode_ == ViewMode::System)
            {
                systemPane_.refresh();
            }
            else if (viewMode_ == ViewMode::Processes)
            {
                processPane_.refresh();
            }
            else if (viewMode_ == ViewMode::Terminal && activeTab_ < (int)tabs_.size())
            {
                tabs_[activeTab_]->refresh();
            }
        }

        renderFrame();
    }

    return 0;
}

void App::shutdown()
{
    for (auto& tab : tabs_)
    {
        tab->shutdown();
    }
    tabs_.clear();
    restoreConsole();
}

void App::handleGlobalKeys(const KeyEvent& key)
{
    // Ctrl+Shift+P: Command Palette.
    if (key.ctrl && key.shift && (key.ch == L'p' || key.ch == L'P' || key.key == Key::P))
    {
        openCommandPalette();
        return;
    }

    // Ctrl+T: New tab.
    if (key.ctrl && !key.shift && (key.ch == L't' || key.ch == L'T' || key.key == Key::T))
    {
        auto pane = std::make_unique<ShellPane>(
            L"terminal-" + std::to_wstring(tabs_.size() + 1));
        std::wstring cwd = (activeTab_ < (int)tabs_.size())
                               ? tabs_[activeTab_]->currentDirectory()
                               : pathutils::getCurrentDirectory();
        pane->initialize(cwd);
        tabs_.push_back(std::move(pane));
        activeTab_ = (int)tabs_.size() - 1;
        viewMode_ = ViewMode::Terminal;
        return;
    }

    // Ctrl+W: Close tab.
    if (key.ctrl && !key.shift && (key.ch == L'w' || key.ch == L'W' || key.key == Key::W))
    {
        if (tabs_.size() > 1)
        {
            tabs_[activeTab_]->shutdown();
            tabs_.erase(tabs_.begin() + activeTab_);
            if (activeTab_ >= (int)tabs_.size())
            {
                activeTab_ = (int)tabs_.size() - 1;
            }
        }
        return;
    }

    // Ctrl+Tab: next tab.
    if (key.ctrl && key.key == Key::Tab)
    {
        if (!key.shift)
        {
            activeTab_ = (activeTab_ + 1) % (int)tabs_.size();
        }
        else
        {
            activeTab_ = (activeTab_ - 1 + (int)tabs_.size()) % (int)tabs_.size();
        }
        viewMode_ = ViewMode::Terminal;
        return;
    }

    // Ctrl+Shift+F: File Manager.
    if (key.ctrl && key.shift && (key.ch == L'f' || key.ch == L'F'))
    {
        switchToView(ViewMode::Files);
        return;
    }

    // Ctrl+Shift+M: Process Manager.
    if (key.ctrl && key.shift && (key.ch == L'm' || key.ch == L'M'))
    {
        switchToView(ViewMode::Processes);
        return;
    }

    // Ctrl+Shift+S: System Monitor.
    if (key.ctrl && key.shift && (key.ch == L's' || key.ch == L'S'))
    {
        switchToView(ViewMode::System);
        return;
    }

    // Ctrl+Shift+G: Git Panel.
    if (key.ctrl && key.shift && (key.ch == L'g' || key.ch == L'G'))
    {
        switchToView(ViewMode::Git);
        return;
    }

    // Ctrl+R: History search in current shell.
    if (key.ctrl && !key.shift && (key.ch == L'r' || key.ch == L'R'))
    {
        openHistorySearch();
        return;
    }

    // Ctrl+Shift+H: History panel.
    if (key.ctrl && key.shift && (key.ch == L'h' || key.ch == L'H'))
    {
        switchToView(ViewMode::History);
        return;
    }

    // Ctrl+Shift+E: Environment panel.
    if (key.ctrl && key.shift && (key.ch == L'e' || key.ch == L'E'))
    {
        switchToView(ViewMode::Environment);
        return;
    }

    // Ctrl+Shift+J: Jobs panel.
    if (key.ctrl && key.shift && (key.ch == L'j' || key.ch == L'J'))
    {
        switchToView(ViewMode::Jobs);
        return;
    }

    // Ctrl+Shift+V: Variables panel.
    if (key.ctrl && key.shift && (key.ch == L'v' || key.ch == L'V'))
    {
        switchToView(ViewMode::Variables);
        return;
    }

    // Ctrl+Shift+T: Trace panel.
    if (key.ctrl && key.shift && (key.ch == L't' || key.ch == L'T'))
    {
        switchToView(ViewMode::Trace);
        return;
    }

    // Ctrl+Shift+1 through 9: Quick switch to sidebar views.
    if (key.ctrl && key.shift)
    {
        if (key.key == Key::Digit1) { switchToView(ViewMode::Terminal); return; }
        if (key.key == Key::Digit2) { switchToView(ViewMode::Files); return; }
        if (key.key == Key::Digit3) { switchToView(ViewMode::Processes); return; }
        if (key.key == Key::Digit4) { switchToView(ViewMode::System); return; }
        if (key.key == Key::Digit5) { switchToView(ViewMode::Git); return; }
        if (key.key == Key::Digit6) { switchToView(ViewMode::Jobs); return; }
        if (key.key == Key::Digit7) { switchToView(ViewMode::History); return; }
        if (key.key == Key::Digit8) { switchToView(ViewMode::Environment); return; }
        if (key.key == Key::Digit9) { switchToView(ViewMode::Variables); return; }
        if (key.key == Key::Digit0) { switchToView(ViewMode::Trace); return; }
    }

    // Ctrl+Shift+L: toggle interface language EN/RU.
    if (key.ctrl && key.shift && (key.ch == L'l' || key.ch == L'L'))
    {
        Locale::instance().toggle();
        return;
    }

    // Escape: if viewing a panel, go back to terminal.
    if (key.key == Key::Escape && viewMode_ != ViewMode::Terminal)
    {
        switchToView(ViewMode::Terminal);
        return;
    }
}

void App::switchToView(ViewMode mode)
{
    viewMode_ = mode;

    // Sync panel context from active shell pane.
    if (activeTab_ < (int)tabs_.size())
    {
        auto& ctx = tabs_[activeTab_]->context();
        jobsPane_.setJobsManager(&ctx.jobs());
        historyPane_.setHistory(&ctx.history());
        envPane_.setEnvironment(&ctx.environment());
        variablesPane_.setTracker(&ctx.variables());
        tracePane_.setTraceLog(&ctx.trace());
        gitPane_.setWorkDir(ctx.currentDirectory());
        filePane_.navigateTo(ctx.currentDirectory());
    }

    // Trigger refresh.
    switch (mode)
    {
    case ViewMode::Files:      filePane_.refresh(); break;
    case ViewMode::Processes:  processPane_.refresh(); break;
    case ViewMode::System:     systemPane_.refresh(); break;
    case ViewMode::Git:        gitPane_.refresh(); break;
    default: break;
    }
}

void App::openCommandPalette()
{
    palette_.setThemeManager(&themeMgr_);
    palette_.setThemeSwitcher([this](const std::wstring& name) {
        themeMgr_.setActiveByName(name);
        config_.themeName = name;
        config_.save();
        if (activeTab_ < (int)tabs_.size())
        {
            tabs_[activeTab_]->context().onThemeChange = [this](const std::wstring& n) {
                themeMgr_.setActiveByName(n);
                config_.themeName = n;
                config_.save();
            };
        }
    });
    palette_.setOnInsert([this](const std::wstring& cmd) {
        if (activeTab_ < (int)tabs_.size())
        {
            tabs_[activeTab_]->setInputText(cmd);
        }
    });
    palette_.open();
}

void App::openHistorySearch()
{
    if (activeTab_ >= (int)tabs_.size())
    {
        return;
    }
    const auto& entries = tabs_[activeTab_]->context().history().entries();
    palette_.setHistoryEntries(entries, [this](const std::wstring& picked) {
        if (activeTab_ < (int)tabs_.size())
        {
            tabs_[activeTab_]->executeFromHistory(picked);
        }
    });
}

KeyEvent App::translateInput(const INPUT_RECORD& rec)
{
    KeyEvent key{};
    const auto& ke = rec.Event.KeyEvent;
    wchar_t ch = ke.uChar.UnicodeChar;
    WORD vk = ke.wVirtualKeyCode;
    WORD fs = ke.dwControlKeyState;

    bool ctrl = (fs & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    bool shift = (fs & SHIFT_PRESSED) != 0;
    bool alt = (fs & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;

    key.ctrl = ctrl;
    key.shift = shift;
    key.alt = alt;

    // Map virtual key codes.
    if (vk >= VK_F1 && vk <= VK_F12)
    {
        key.key = static_cast<Key>(static_cast<int>(Key::F1) + (vk - VK_F1));
        return key;
    }

    if (vk >= 'A' && vk <= 'Z')
    {
        key.ch = ch;
        key.key = static_cast<Key>(static_cast<int>(Key::A) + (vk - 'A'));
        return key;
    }
    if (vk >= '0' && vk <= '9')
    {
        key.ch = ch;
        key.key = static_cast<Key>(static_cast<int>(Key::Digit0) + (vk - '0'));
        return key;
    }

    switch (vk)
    {
    case VK_RETURN:    key.key = Key::Enter; key.ch = L'\r'; return key;
    case VK_TAB:       key.key = Key::Tab; key.ch = L'\t'; return key;
    case VK_BACK:      key.key = Key::Backspace; key.ch = L'\b'; return key;
    case VK_DELETE:    key.key = Key::Delete; return key;
    case VK_ESCAPE:    key.key = Key::Escape; key.ch = L'\x1b'; return key;
    case VK_SPACE:     key.key = Key::Space; key.ch = L' '; return key;
    case VK_UP:        key.key = Key::Up; return key;
    case VK_DOWN:      key.key = Key::Down; return key;
    case VK_LEFT:      key.key = Key::Left; return key;
    case VK_RIGHT:     key.key = Key::Right; return key;
    case VK_HOME:      key.key = Key::Home; return key;
    case VK_END:       key.key = Key::End; return key;
    case VK_PRIOR:     key.key = Key::PageUp; return key;
    case VK_NEXT:      key.key = Key::PageDown; return key;
    case VK_INSERT:    key.key = Key::Insert; return key;
    default: break;
    }

    // Fallback: any other key that produced a printable character is treated
    // as text input (punctuation, symbols, accented letters, etc.). Previously
    // these characters (e.g. '-', ':', '/', '.') were silently dropped, which
    // made it impossible to type many things in the shell.
    if (ch >= 0x20 && ch != 0x7f)
    {
        key.ch = ch;
    }

    return key;
}

void App::handleMouseEvent(const MOUSE_EVENT_RECORD& e)
{
    // Mouse wheel scrolling.
    if (e.dwEventFlags & MOUSE_WHEELED)
    {
        SHORT wheel = HIWORD(e.dwButtonState);
        int delta = (wheel > 0) ? 1 : -1;
        scrollContent(delta);
        return;
    }

    // Mouse click selection (left button press).
    if (e.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
    {
        bool isDouble = (e.dwEventFlags & DOUBLE_CLICK) != 0;
        handleMouseClick(e.dwMousePosition.Y, e.dwMousePosition.X, isDouble);
    }
}

void App::handleMouseClick(int row, int col, bool doubleClick)
{
    // Layout constants (must mirror renderFrame).
    int titleH = 1;
    int tabBarH = 1;
    int statusH = 1;
    int mainY = titleH + tabBarH;

    // Click on the language indicator in the top-right to toggle EN/RU.
    if (row == 0 && !doubleClick)
    {
        std::wstring langLabel = std::wstring(L" [") + Locale::instance().code() + L"]";
        int langX = termWidth_ - (int)langLabel.size();
        if (langX > 0 && col >= langX && col < termWidth_)
        {
            Locale::instance().toggle();
            return;
        }
    }

    // Click on a sidebar item to switch views.
    if (col >= 0 && col < sidebarWidth_ && row >= mainY && row < mainY + 10)
    {
        ViewMode modes[] = {
            ViewMode::Terminal, ViewMode::Files, ViewMode::Processes, ViewMode::System,
            ViewMode::Git, ViewMode::Jobs, ViewMode::History, ViewMode::Environment,
            ViewMode::Variables, ViewMode::Trace
        };
        int idx = row - mainY;
        if (idx >= 0 && idx < 10)
        {
            switchToView(modes[idx]);
        }
        return;
    }

    // Click on a tab in the tab bar to switch tabs.
    if (row == titleH)
    {
        int x = 1;
        for (int i = 0; i < (int)tabs_.size(); ++i)
        {
            std::wstring label = L" " + std::to_wstring(i + 1) + L":" + tabs_[i]->title() + L" ";
            int w = (int)label.size();
            if (col >= x && col < x + w)
            {
                activeTab_ = i;
                viewMode_ = ViewMode::Terminal;
                return;
            }
            x += w;
        }
    }

    // Route click into the active panel's content area.
    int contentX = sidebarWidth_ + 1;
    if (row >= mainY && col >= contentX) // clicks inside a panel
    {
        int rowInPane = row - mainY;
        int colInPane = col - contentX;
        if (palette_.isOpen())
        {
            palette_.onMouseClick(rowInPane, colInPane, doubleClick);
            return;
        }
        switch (viewMode_)
        {
        case ViewMode::Files:      filePane_.onMouseClick(rowInPane, colInPane, doubleClick); break;
        case ViewMode::Processes:  processPane_.onMouseClick(rowInPane, colInPane, doubleClick); break;
        default: break;
        }
    }
}

void App::scrollContent(int delta)
{
    // Command palette handles its own scroll.
    if (palette_.isOpen())
    {
        return;
    }

    switch (viewMode_)
    {
    case ViewMode::Terminal:
        if (activeTab_ < (int)tabs_.size())
        {
            tabs_[activeTab_]->onMouseWheel(delta);
        }
        break;
    case ViewMode::Files:      filePane_.onMouseWheel(delta); break;
    case ViewMode::Processes:  processPane_.onMouseWheel(delta); break;
    case ViewMode::System:     systemPane_.onMouseWheel(delta); break;
    case ViewMode::Git:        gitPane_.onMouseWheel(delta); break;
    case ViewMode::Jobs:       jobsPane_.onMouseWheel(delta); break;
    case ViewMode::History:    historyPane_.onMouseWheel(delta); break;
    case ViewMode::Environment: envPane_.onMouseWheel(delta); break;
    case ViewMode::Variables:  variablesPane_.onMouseWheel(delta); break;
    case ViewMode::Trace:      tracePane_.onMouseWheel(delta); break;
    }
}

void App::renderFrame()
{
    const auto& t = themeMgr_.activeTheme();
    RenderContext rc{t, screen_, {0, 0, termWidth_, termHeight_}};

    // Clear screen.
    screen_.clear(t.color(render::Role::Background), t.color(render::Role::Foreground));

    // Layout: 1 row title, 1 row tab bar, main area, 1 row status.
    int titleH = 1;
    int tabBarH = 1;
    int statusH = 1;
    int mainH = termHeight_ - titleH - tabBarH - statusH;
    if (mainH < 1)
    {
        mainH = 1;
    }
    int mainY = titleH + tabBarH;

    // Title bar.
    rc.bounds = {0, 0, termWidth_, titleH};
    draw::header(rc, L"KShell " KSHELL_VERSION);

    // Language toggle indicator on the right side of the title bar.
    {
        const auto& bg = t.color(render::Role::Sidebar);
        const auto& fg = t.color(render::Role::Accent);
        std::wstring langLabel = std::wstring(L" [") + Locale::instance().code() + L"]";
        int langX = termWidth_ - (int)langLabel.size();
        if (langX > 0)
        {
            screen_.putText(0, langX, langLabel, fg, bg, true);
        }
    }

    // Tab bar.
    rc.bounds = {0, titleH, termWidth_, tabBarH};
    {
        const auto& bg = t.color(render::Role::Tab);
        const auto& fg = t.color(render::Role::Foreground);
        const auto& accent = t.color(render::Role::Accent);
        screen_.fillLine(titleH, 0, termWidth_, L' ', bg, bg);
        int x = 1;
        for (int i = 0; i < (int)tabs_.size(); ++i)
        {
            std::wstring label = L" " + std::to_wstring(i + 1) + L":" + tabs_[i]->title() + L" ";
            bool isActive = (i == activeTab_ && viewMode_ == ViewMode::Terminal);
            auto col = isActive ? accent : fg;
            screen_.putText(titleH, x, label, col, bg, isActive);
            x += (int)label.size();
        }
        // View indicator.
        if (viewMode_ != ViewMode::Terminal)
        {
            std::wstring viewLabel = L" [" + std::wstring(kViewNames[static_cast<int>(viewMode_)]) + L"]";
            screen_.putText(titleH, x, viewLabel,
                            t.color(render::Role::Warning), bg, true);
        }
    }

    // Main content area.
    int sidebarW = sidebarWidth_;
    int contentX = sidebarW + 1;
    int contentW = termWidth_ - contentX;
    if (contentW < 1)
    {
        contentW = 1;
    }

    // Sidebar.
    {
        rc.bounds = {0, mainY, sidebarW, mainH};
        draw::clear(rc, render::Role::Sidebar);

        const wchar_t* items[] = {
            L"Terminal", L"Files", L"Processes", L"System",
            L"Git", L"Jobs", L"History", L"Environment", L"Variables", L"Trace"
        };
        ViewMode modes[] = {
            ViewMode::Terminal, ViewMode::Files, ViewMode::Processes, ViewMode::System,
            ViewMode::Git, ViewMode::Jobs, ViewMode::History, ViewMode::Environment,
            ViewMode::Variables, ViewMode::Trace
        };
        int itemCount = 10;
        for (int i = 0; i < itemCount && i < mainH; ++i)
        {
            bool selected = (modes[i] == viewMode_);
            auto col = selected
                           ? t.color(render::Role::SidebarActive)
                           : t.color(render::Role::SidebarText);
            std::wstring label = L" " + tr(items[i]);
            if ((int)label.size() > sidebarW)
            {
                label = label.substr(0, sidebarW);
            }
            screen_.putText(mainY + i, 0, label, col,
                            selected ? t.color(render::Role::Selection)
                                     : t.color(render::Role::Sidebar));
        }
    }

    // Vertical separator.
    {
        auto borderFg = t.color(render::Role::Border);
        auto borderBg = t.color(render::Role::Background);
        for (int i = 0; i < mainH; ++i)
        {
            screen_.put(mainY + i, sidebarW, L'\u2502', borderFg, borderBg);
        }
    }

    // Content.
    switch (viewMode_)
    {
    case ViewMode::Terminal:
    {
        if (activeTab_ < (int)tabs_.size())
        {
            rc.bounds = {contentX, mainY, contentW, mainH};
            tabs_[activeTab_]->draw(rc);
        }
        break;
    }
    case ViewMode::Files:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        filePane_.draw(rc);
        break;
    }
    case ViewMode::Processes:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        processPane_.draw(rc);
        break;
    }
    case ViewMode::System:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        systemPane_.draw(rc);
        break;
    }
    case ViewMode::Git:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        gitPane_.draw(rc);
        break;
    }
    case ViewMode::Jobs:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        jobsPane_.draw(rc);
        break;
    }
    case ViewMode::History:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        historyPane_.draw(rc);
        break;
    }
    case ViewMode::Environment:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        envPane_.draw(rc);
        break;
    }
    case ViewMode::Variables:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        variablesPane_.draw(rc);
        break;
    }
    case ViewMode::Trace:
    {
        rc.bounds = {contentX, mainY, contentW, mainH};
        tracePane_.draw(rc);
        break;
    }
    }

    // Status bar.
    {
        rc.bounds = {0, termHeight_ - statusH, termWidth_, statusH};
        const auto& bg = t.color(render::Role::StatusBar);
        const auto& fg = t.color(render::Role::StatusBarText);
        screen_.fillLine(termHeight_ - statusH, 0, termWidth_, L' ', bg, bg);

        std::wstring cwd;
        if (activeTab_ < (int)tabs_.size())
        {
            cwd = tabs_[activeTab_]->currentDirectory();
        }
        // Truncate CWD if too long.
        int maxCwdW = termWidth_ - 40;
        if (maxCwdW > 0 && (int)cwd.size() > maxCwdW)
        {
            cwd = L"..." + cwd.substr(cwd.size() - maxCwdW + 3);
        }
        screen_.putText(termHeight_ - statusH, 1, cwd, fg, bg);

        // Right side: tab count, view.
        std::wstring right = tr(L"Tab") + L" " + std::to_wstring(activeTab_ + 1) +
                             L"/" + std::to_wstring(tabs_.size());
        if (viewMode_ != ViewMode::Terminal)
        {
            right += L"  |  " + tr(kViewNames[static_cast<int>(viewMode_)]);
        }
        screen_.putText(termHeight_ - statusH, termWidth_ - (int)right.size() - 1,
                        right, fg, bg);
    }

    // Command palette overlay (draws on top).
    if (palette_.isOpen())
    {
        rc.bounds = {0, 0, termWidth_, termHeight_};
        palette_.draw(rc);
    }

    // Flush to console via ANSI escape sequences.
    std::wstring output;
    output.reserve(termWidth_ * termHeight_ * 4);

    render::Color lastFg{};
    render::Color lastBg{};
    bool lastBold = false;
    bool hasLastFg = false;
    bool hasLastBg = false;

    const auto& cells = screen_.front();
    for (int row = 0; row < termHeight_; ++row)
    {
        output += ansiMove(row + 1, 1);
        for (int col = 0; col < termWidth_; ++col)
        {
            const auto& cell = cells[(size_t)row * termWidth_ + col];
            if (!hasLastFg || cell.fg != lastFg)
            {
                output += ansi24(cell.fg);
                lastFg = cell.fg;
                hasLastFg = true;
            }
            if (!hasLastBg || cell.bg != lastBg)
            {
                output += ansi24bg(cell.bg);
                lastBg = cell.bg;
                hasLastBg = true;
            }
            if (cell.bold != lastBold)
            {
                output += ansiBold(cell.bold);
                lastBold = cell.bold;
            }
            output += cell.ch;
        }
    }
    output += ansiReset();

    DWORD written = 0;
    ::WriteConsoleW(hOut_, output.c_str(), (DWORD)output.size(), &written, nullptr);
}

} // namespace kshell::ui
