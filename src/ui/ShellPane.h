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

    ShellContext   ctx_;
    std::unique_ptr<IOutputSink> bufferSink_;
    OutputBuffer   buffer_;
    Autocomplete   autocomplete_;
    std::wstring   input_;
    size_t         inputCursor_ = 0;
    bool           initialized_ = false;
    bool           exitRequested_ = false;
    int            scrollOffset_ = 0;  // from bottom, 0 = show most recent

    // History navigation state
    int            historySearchIdx_ = -1;
    std::wstring   savedInput_;
};

} // namespace kshell::ui
