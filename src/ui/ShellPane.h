#pragma once

#include "ui/Ui.h"
#include "ui/Key.h"
#include "ui/Fuzzy.h"
#include "terminal/IOutputSink.h"
#include "core/ShellContext.h"
#include "parser/Lexer.h"
#include "parser/Parser.h"
#include "autocomplete/Autocomplete.h"

#include <string>
#include <vector>
#include <memory>

namespace kshell::ui
{

// An interactive shell session rendered as a pane inside the TUI.
// Each tab has its own ShellPane with its own ShellContext, history state,
// and output buffer.
class ShellPane : public Pane
{
public:
    explicit ShellPane(std::wstring id = L"terminal");
    ~ShellPane() override;

    bool initialize(const std::wstring& startupDir = L"");
    void shutdown();

    void draw(RenderContext& rc) override;
    bool onKey(const KeyEvent& key) override;
    void onMouseWheel(int delta) override;
    void refresh() override;

    // Mouse text selection + clipboard:
    // Press-and-drag selects lines from the scrollback buffer; releasing the
    // left button copies the selection to the clipboard. Right-click pastes
    // clipboard content into the input line. Coordinates are pane-relative
    // (row 0 = top line of the pane, col 0 = first column).
    void onMousePress(int rowInPane, int colInPane);
    void onMouseDrag(int rowInPane, int colInPane);
    void onMouseRelease(int rowInPane, int colInPane);
    void onMousePaste(int rowInPane, int colInPane);

    ShellContext& context() { return ctx_; }
    OutputBuffer& buffer() { return buffer_; }

    std::wstring currentDirectory() const;
    const std::wstring& inputLine() const { return input_; }
    void setInputText(const std::wstring& text) { input_ = text; inputCursor_ = input_.size(); }
    void executeCommand(const std::wstring& line);

    // Re-executes a line from history (for Ctrl+R selection).
    void executeFromHistory(const std::wstring& line);

private:
    void printPromptText();

    // Map a pane-relative coordinate to (buffer line index, column).
    void mapToBuffer(int rowInPane, int colInPane, int& lineIdx, int& colIdx);
    // Gather the selected lines (anchor..cursor) into plain text.
    std::wstring selectedText();
    // Copy text to the Windows clipboard.
    static void copyToClipboard(const std::wstring& text);
    // Read current clipboard text.
    static std::wstring clipboardText();
    // Insert text into the input line at the cursor and schedule a redraw.
    void insertInput(const std::wstring& text);

    ShellContext   ctx_;
    std::unique_ptr<IOutputSink> bufferSink_;
    OutputBuffer   buffer_;
    Autocomplete   autocomplete_;
    std::wstring   input_;
    size_t         inputCursor_ = 0;
    bool           initialized_ = false;
    bool           exitRequested_ = false;
    int            scrollOffset_ = 0;  // from bottom, 0 = show most recent

    // Last draw geometry (used to map mouse co-ordinates to buffer lines).
    int            firstVisible_ = 0;
    int            paneW_ = 0;
    int            paneH_ = 0;

    // Mouse text selection state (buffer-line coordinates).
    bool           selecting_ = false;
    int            selAnchorLine_ = -1;
    int            selAnchorCol_ = -1;
    int            selCursorLine_ = -1;
    int            selCursorCol_ = -1;

    // History navigation state
    int            historySearchIdx_ = -1;
    std::wstring   savedInput_;
};

} // namespace kshell::ui
