#pragma once

#include "ui/Ui.h"
#include "ui/Key.h"
#include "ui/ShellPane.h"
#include "ui/CommandPalette.h"
#include "ui/Panels.h"
#include "render/Screen.h"
#include "render/ThemeManager.h"
#include "config/Config.h"
#include "system/SystemMonitor.h"

#include <windows.h>

#include <memory>
#include <vector>
#include <string>

namespace kshell::ui
{

enum class ViewMode
{
    Terminal,
    Files,
    Processes,
    System,
    Git,
    Jobs,
    History,
    Environment,
    Variables,
    Trace,
};

class App
{
public:
    App();
    ~App();

    bool initialize(int argc, wchar_t* argv[]);
    int  run();
    void shutdown();

private:
    void handleGlobalKeys(const KeyEvent& key);
    void renderFrame();
    void switchToView(ViewMode mode);
    void openCommandPalette();
    void openHistorySearch();

    // Console management.
    bool initConsole();
    void restoreConsole();

    KeyEvent translateInput(const INPUT_RECORD& rec);
    void     handleMouseEvent(const MOUSE_EVENT_RECORD& e);
    void     handleMouseClick(int row, int col, bool doubleClick);
    void     scrollContent(int delta);

    Config            config_;
    render::Screen    screen_;
    render::ThemeManager themeMgr_;
    bool              running_ = false;
    ViewMode          viewMode_ = ViewMode::Terminal;
    int               termWidth_ = 0;
    int               termHeight_ = 0;

    // Views.
    std::vector<std::unique_ptr<ShellPane>> tabs_;
    int  activeTab_ = 0;

    FilePane         filePane_;
    ProcessPane      processPane_;
    SystemPane       systemPane_;
    GitPane          gitPane_;
    JobsPane         jobsPane_;
    HistoryPane      historyPane_;
    EnvPane          envPane_;
    VariablesPane    variablesPane_;
    TracePane        tracePane_;

    CommandPalette   palette_;

    // Sidebar state.
    int  sidebarWidth_ = 24;
    int  sidebarSelected_ = 0;

    // Mouse text selection state (left-button drag inside the terminal pane).
    bool   mouseDrag_ = false;

    // Console handles.
    HANDLE hIn_ = INVALID_HANDLE_VALUE;
    HANDLE hOut_ = INVALID_HANDLE_VALUE;
    DWORD  savedInMode_ = 0;
    DWORD  savedOutMode_ = 0;
    bool   altScreen_ = false;
    bool   vtProcessing_ = false;
};

} // namespace kshell::ui
