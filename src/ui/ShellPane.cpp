#include "ui/ShellPane.h"

#include "builtin/BuiltinRegistry.h"
#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "ui/ExecEngine.h"
#include "config/Config.h"
#include "core/Locale.h"

#include <windows.h>

#include <sstream>
#include <algorithm>

namespace kshell::ui
{

namespace
{

// Output sink that appends to a pane's scrollback buffer.
class BufferSink : public IOutputSink
{
public:
    explicit BufferSink(OutputBuffer& buf, const Config& cfg) : buf_(buf), cfg_(cfg) {}
    void print(const std::wstring& text) override
    {
        buf_.appendText(text);
    }
    void printLine(const std::wstring& text) override
    {
        buf_.append(text);
    }
    void printPrompt(const std::wstring& prompt, const ColorSettings&) override
    {
        buf_.append(prompt);
    }
    void printError(const std::wstring& text) override
    {
        buf_.append(L"[error] " + text);
    }
    void printSuccess(const std::wstring& text) override
    {
        buf_.append(text);
    }
private:
    OutputBuffer& buf_;
    const Config& cfg_;
};

// Resolve aliases for the first token of a command.
std::vector<std::wstring> resolveAliases(const std::vector<std::wstring>& tokens,
                                         const std::map<std::wstring, std::wstring>& aliases)
{
    if (tokens.empty())
    {
        return tokens;
    }
    const auto it = aliases.find(tokens[0]);
    if (it == aliases.end())
    {
        return tokens;
    }
    auto expanded = stringutils::splitCommandLine(it->second);
    if (expanded.empty())
    {
        return tokens;
    }
    for (size_t i = 1; i < tokens.size(); ++i)
    {
        expanded.push_back(tokens[i]);
    }
    return expanded;
}

// Try to find a builtin function matching the program name.
BuiltinFunction findBuiltin(const std::wstring& name)
{
    return builtinLookup(name);
}

// Resolve external executable path.
std::wstring resolveExe(const std::wstring& name, const std::wstring& pathVar)
{
    auto dirs = pathutils::getPathDirectories(pathVar);
    for (const auto& d : dirs)
    {
        for (const std::wstring& ext : {L"", L".exe", L".com", L".bat", L".cmd"})
        {
            std::wstring full = d + L"\\" + name + ext;
            if (pathutils::pathExists(full))
            {
                return full;
            }
        }
    }
    return name;
}

// Build a command line from program + args, quoting as needed.
std::wstring buildCmdLine(const std::wstring& prog, const std::vector<std::wstring>& args)
{
    auto quoteArg = [](const std::wstring& a) -> std::wstring {
        if (a.find_first_of(L" \t\"") == std::wstring::npos)
        {
            return a;
        }
        return L"\"" + a + L"\"";
    };
    std::wstring cmd = quoteArg(prog);
    for (const auto& a : args)
    {
        cmd += L" " + quoteArg(a);
    }
    return cmd;
}

// Lower-case extension of a file path (without the dot).
std::wstring fileExtensionLower(const std::wstring& path)
{
    const auto pos = path.rfind(L'.');
    if (pos == std::wstring::npos || pos == path.size() - 1)
    {
        return {};
    }
    std::wstring ext = path.substr(pos + 1);
    for (auto& c : ext)
    {
        c = (wchar_t)std::towlower(c);
    }
    return ext;
}

// Scan captured output for lines "set NAME=value" and feed them into the
// shell environment + variable tracker so that a script's assignments
// become visible in the Variables pane even though the child process
// manages its own environment.
void captureSetLines(const std::wstring& output, ShellContext& ctx)
{
    size_t pos = 0;
    while (pos < output.size())
    {
        size_t nl = output.find(L'\n', pos);
        if (nl == std::wstring::npos) { nl = output.size(); }
        std::wstring line = output.substr(pos, nl - pos);
        if (!line.empty() && line.back() == L'\r') { line.pop_back(); }
        std::wstring t = stringutils::trim(line);
        if (t.size() >= 4 && t.substr(0, 4) == L"set ")
        {
            const auto eq = t.find(L'=');
            if (eq != std::wstring::npos)
            {
                const std::wstring name  = t.substr(4, eq - 4);
                const std::wstring value = t.substr(eq + 1);
                if (!name.empty())
                {
                    ctx.environment().set(name, value);
                    ctx.variables().recordSet(name, value);
                }
            }
        }
        pos = nl + 1;
    }
}

} // namespace

ShellPane::ShellPane(std::wstring id)
    : Pane(std::move(id))
{
    title_ = L"Terminal";
}

ShellPane::~ShellPane()
{
    shutdown();
}

bool ShellPane::initialize(const std::wstring& startupDir)
{
    if (initialized_)
    {
        return true;
    }

    // Create a buffer sink and attach it to the context.
    bufferSink_ = std::make_unique<BufferSink>(buffer_, ctx_.config());
    ctx_.setOutputSink(bufferSink_.get());

    if (!ctx_.initialize())
    {
        return false;
    }

    if (!startupDir.empty())
    {
        ctx_.setCurrentDirectory(startupDir);
    }

    // Set up autocomplete.
    std::vector<std::wstring> builtinNames;
    for (const auto& reg : builtinRegistry())
    {
        builtinNames.push_back(reg.name);
    }
    autocomplete_.setBuiltinNames(builtinNames);

    // Aliases are fetched on-demand during Tab completion (see onKey Tab handler).

    initialized_ = true;

    // Print welcome message.
    buffer_.append(L"KShell " KSHELL_VERSION L" (C++20 / Windows x64)");
    buffer_.append(tr(L"Type \"help\" for available commands."));
    buffer_.append(L"");

    printPromptText();
    return true;
}

void ShellPane::shutdown()
{
    if (initialized_)
    {
        ctx_.history().saveToFile(ctx_.config().getHistoryFilePath());
        initialized_ = false;
    }
}

std::wstring ShellPane::currentDirectory() const
{
    return ctx_.currentDirectory();
}

void ShellPane::printPromptText()
{
    std::wstring prompt = ctx_.promptText();
    buffer_.append(prompt);
}

void ShellPane::executeCommand(const std::wstring& line)
{
    ctx_.outputSink()->print(line + L"\n");

    std::wstring trimmed = stringutils::trim(line);
    if (trimmed.empty())
    {
        printPromptText();
        return;
    }

    ctx_.history().add(trimmed);

    // Command trace (set -x style): record the raw and expanded command.
    ctx_.trace().record(TraceKind::CommandTrace, L"+ " + trimmed);

    // Variable expansion.
    std::wstring expanded = trimmed;
    ctx_.environment().refresh();
    expanded = ctx_.environment().expand(expanded);

    if (expanded != trimmed)
    {
        ctx_.trace().record(TraceKind::CommandTrace, L"expanded: " + expanded);
    }

    // Parse.
    std::optional<ParseError> error;
    Lexer lexer(expanded);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto pipelines = parser.parse(error);

    if (error)
    {
        ctx_.outputSink()->printError(L"Parse error: " + error->message);
        printPromptText();
        return;
    }

    for (auto& pipeline : pipelines)
    {
        // Resolve aliases for each command in the pipeline.
        auto aliases = ctx_.config().getAliases();
        for (auto& command : pipeline.commands)
        {
            std::vector<std::wstring> cmdTokens;
            cmdTokens.push_back(command.program);
            for (const auto& arg : command.arguments)
            {
                cmdTokens.push_back(arg);
            }
            auto resolved = resolveAliases(cmdTokens, aliases);
            if (!resolved.empty())
            {
                command.program = resolved[0];
                command.arguments.clear();
                for (size_t i = 1; i < resolved.size(); ++i)
                {
                    command.arguments.push_back(resolved[i]);
                }
            }
        }

        // Check if all commands are builtins (simple single-command case).
        bool allBuiltin = true;
        for (const auto& cmd : pipeline.commands)
        {
            if (!findBuiltin(cmd.program))
            {
                allBuiltin = false;
                break;
            }
        }

        // Run a single builtin if present. A builtin may return handled == false
        // to request fall-through to external resolution (e.g. the `git`
        // passthrough); in that case we must NOT swallow the command here.
        bool builtinHandled = false;
        if (pipeline.commands.size() == 1 && allBuiltin)
        {
            auto builtinFn = findBuiltin(pipeline.commands[0].program);
            if (builtinFn)
            {
                std::vector<std::wstring> fullArgs;
                fullArgs.push_back(pipeline.commands[0].program);
                for (const auto& a : pipeline.commands[0].arguments)
                {
                    fullArgs.push_back(a);
                }
                auto result = builtinFn(ctx_, fullArgs);
                if (result.exitRequested)
                {
                    ctx_.outputSink()->printLine(L"Goodbye.");
                    exitRequested_ = true;
                }
                builtinHandled = result.handled;
            }
        }
        if (builtinHandled)
        {
            continue;
        }

        // External command execution (with capture).
        ctx_.refreshExecutor();
        if (pipeline.commands.size() == 1)
        {
            const auto& cmd = pipeline.commands[0];
            std::wstring cwd = ctx_.currentDirectory();

            // Detect script file extensions and wrap the command accordingly.
            const std::wstring ext = fileExtensionLower(cmd.program);
            std::wstring exe;
            std::wstring cmdLine;

            if (ext == L"bat" || ext == L"cmd")
            {
                // Run batch scripts through cmd.exe.
                exe = L"C:\\Windows\\System32\\cmd.exe";
                cmdLine = L"/c " + buildCmdLine(cmd.program, cmd.arguments);
            }
            else if (ext == L"ps1")
            {
                // Run PowerShell scripts.
                exe = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
                std::wstring inner = buildCmdLine(cmd.program, cmd.arguments);
                cmdLine = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + inner + L"\"";
            }
            else
            {
                exe = resolveExe(cmd.program, ctx_.environment().getPath());
                cmdLine = buildCmdLine(cmd.program, cmd.arguments);
            }

            auto result = ExecEngine::runExternal(exe, cmdLine, cwd, L"");
            ctx_.trace().recordExecution(cmdLine, result.pid, result.exitCode,
                                         result.durationMs, result.stdoutText);

            // Capture any "set X=Y" lines from the output and feed them
            // into the environment + variable tracker so they appear in
            // the Variables pane.
            captureSetLines(result.stdoutText, ctx_);
            captureSetLines(result.stderrText, ctx_);

            if (!result.stdoutText.empty())
            {
                while (!result.stdoutText.empty() &&
                       (result.stdoutText.back() == L'\n' || result.stdoutText.back() == L'\r'))
                {
                    result.stdoutText.pop_back();
                }
                ctx_.outputSink()->print(result.stdoutText + L"\n");
            }
            if (!result.stderrText.empty())
            {
                while (!result.stderrText.empty() &&
                       (result.stderrText.back() == L'\n' || result.stderrText.back() == L'\r'))
                {
                    result.stderrText.pop_back();
                }
                // stderr не всегда означает ошибку: многие программы (например
                // `git push`) пишут прогресс именно в stderr даже при успехе.
                // Как ошибку помечаем только при реальном провале команды.
                if (result.succeeded)
                {
                    ctx_.outputSink()->print(result.stderrText + L"\n");
                }
                else
                {
                    ctx_.outputSink()->printError(result.stderrText);
                }
            }
            if (!result.succeeded && result.pid == 0)
            {
                ctx_.outputSink()->printError(L"Command not found: " + cmd.program);
            }
            if (result.cancelled)
            {
                ctx_.outputSink()->print(L"[interrupted] Command stopped (Esc / Ctrl+C)\n");
            }
        }
        else if (pipeline.commands.size() == 2)
        {
            const auto& cmd1 = pipeline.commands[0];
            const auto& cmd2 = pipeline.commands[1];
            std::wstring cwd = ctx_.currentDirectory();
            std::wstring envBlock = L"";
            auto result = ExecEngine::runPipeline2(
                cmd1.program, buildCmdLine(cmd1.program, cmd1.arguments),
                cmd2.program, buildCmdLine(cmd2.program, cmd2.arguments),
                cwd, envBlock);
            ctx_.trace().recordExecution(buildCmdLine(cmd1.program, cmd1.arguments) + L" | " +
                                             buildCmdLine(cmd2.program, cmd2.arguments),
                                         result.pid, result.exitCode,
                                         result.durationMs, result.stdoutText);
            if (!result.stdoutText.empty())
            {
                while (!result.stdoutText.empty() &&
                       (result.stdoutText.back() == L'\n' || result.stdoutText.back() == L'\r'))
                {
                    result.stdoutText.pop_back();
                }
                ctx_.outputSink()->print(result.stdoutText + L"\n");
            }
            if (result.cancelled)
            {
                ctx_.outputSink()->print(L"[interrupted] Command stopped (Esc / Ctrl+C)\n");
            }
        }
        else
        {
            // Multi-command or mixed: just run first command for now.
            if (!pipeline.commands.empty())
            {
                const auto& cmd = pipeline.commands[0];
                std::wstring cmdLine = buildCmdLine(cmd.program, cmd.arguments);
                std::wstring cwd = ctx_.currentDirectory();
                auto result = ExecEngine::runExternal(resolveExe(cmd.program, ctx_.environment().getPath()),
                                                     cmdLine, cwd, L"");
                ctx_.trace().recordExecution(cmdLine, result.pid, result.exitCode,
                                             result.durationMs, result.stdoutText);
                if (!result.stdoutText.empty())
                {
                    while (!result.stdoutText.empty() &&
                           (result.stdoutText.back() == L'\n' || result.stdoutText.back() == L'\r'))
                    {
                        result.stdoutText.pop_back();
                    }
                    ctx_.outputSink()->print(result.stdoutText + L"\n");
                }
                if (result.cancelled)
                {
                    ctx_.outputSink()->print(L"[interrupted] Command stopped (Esc / Ctrl+C)\n");
                }
            }
        }
    }

    ctx_.jobs().reaper();
    buffer_.trimTo(10000);
    if (!exitRequested_)
    {
        printPromptText();
    }
}

void ShellPane::executeFromHistory(const std::wstring& line)
{
    executeCommand(line);
}

void ShellPane::draw(RenderContext& rc)
{
    const auto& t = rc.theme;
    const render::Color bg = t.color(render::Role::Background);
    const render::Color fg = t.color(render::Role::Foreground);
    const render::Color accent = t.color(render::Role::Accent);
    const render::Color muted = t.color(render::Role::Muted);

    // Clear background.
    rc.screen.fillRect(rc.bounds.y, rc.bounds.x, rc.bounds.h, rc.bounds.w,
                       L' ', bg, bg);

    // Draw scrollback buffer lines.
    buffer_.lock();
    const auto& lines = buffer_.lines();
    int totalLines = (int)lines.size();
    int viewH = rc.bounds.h - 1;  // reserve 1 line for input
    if (viewH < 1)
    {
        viewH = 1;
    }

    int firstVisible = 0;
    if (totalLines > viewH - 1)
    {
        int maxScroll = totalLines - (viewH - 1);
        if (scrollOffset_ > maxScroll)
        {
            scrollOffset_ = maxScroll;
        }
        firstVisible = totalLines - (viewH - 1) - scrollOffset_;
    }
    else
    {
        scrollOffset_ = 0;
    }

    int row = 0;
    // Keep the last-rendered geometry so mouse events can map screen
    // co-ordinates back to scrollback buffer lines.
    firstVisible_ = firstVisible;
    paneW_ = rc.bounds.w;
    paneH_ = rc.bounds.h;

    for (int i = firstVisible; i < totalLines && row < viewH; ++i, ++row)
    {
        const auto& line = lines[i];
        // Determine if this line is a prompt or output.
        bool isPrompt = (line.size() > 0 && line.back() == L'>' && line[line.size()-1] == L' ');
        render::Color textFg = fg;
        if (line.find(L"[error]") == 0)
        {
            textFg = t.color(render::Role::Error);
        }
        else if (line.find(L"KShell") == 0)
        {
            textFg = accent;
        }
        else if (line.find(L"Goodbye") != std::wstring::npos)
        {
            textFg = muted;
        }

        // Draw the line text, clipped to bounds.
        int maxW = rc.bounds.w;
        std::wstring display = line;
        if ((int)display.size() > maxW)
        {
            display = display.substr(0, maxW);
        }

        // Compute the portion of this line covered by the mouse selection.
        int selColStart = 0;
        int selColEnd = 0;
        bool hasSelOnLine = false;
        if (selAnchorLine_ >= 0 && selCursorLine_ >= 0)
        {
            int aL = selAnchorLine_, cL = selCursorLine_;
            int l0 = std::min(aL, cL);
            int l1 = std::max(aL, cL);
            if (i >= l0 && i <= l1)
            {
                int c0 = (i == l0) ? selAnchorCol_ : selCursorCol_;
                int c1 = (i == l1) ? selAnchorCol_ : selCursorCol_;
                if (l0 == l1 && c0 > c1)
                {
                    std::swap(c0, c1);
                }
                if (i == l0 && i != l1)
                {
                    selColStart = c0;
                    selColEnd = (int)display.size();
                }
                else if (i == l1 && i != l0)
                {
                    selColStart = 0;
                    selColEnd = std::min(c1, (int)display.size());
                }
                else if (i == l0 && i == l1)
                {
                    selColStart = c0;
                    selColEnd = std::min(c1, (int)display.size());
                }
                else
                {
                    selColStart = 0;
                    selColEnd = (int)display.size();
                }
                if (selColStart > (int)display.size())
                {
                    selColStart = (int)display.size();
                }
                if (selColEnd < selColStart)
                {
                    selColEnd = selColStart;
                }
                if (selColEnd > (int)display.size())
                {
                    selColEnd = (int)display.size();
                }
                hasSelOnLine = selColEnd > selColStart;
            }
        }

        if (!hasSelOnLine)
        {
            rc.screen.putText(rc.bounds.y + row, rc.bounds.x, display,
                              textFg, bg);
        }
        else
        {
            // Draw the selected range with the selection background.
            const auto selFg = t.color(render::Role::Foreground);
            const auto selBg = t.color(render::Role::Selection);
            if (selColStart > 0)
            {
                rc.screen.putText(rc.bounds.y + row, rc.bounds.x,
                                  display.substr(0, selColStart), textFg, bg);
            }
            rc.screen.putText(rc.bounds.y + row, rc.bounds.x + selColStart,
                              display.substr(selColStart, selColEnd - selColStart),
                              selFg, selBg);
            if (selColEnd < (int)display.size())
            {
                rc.screen.putText(rc.bounds.y + row, rc.bounds.x + selColEnd,
                                  display.substr(selColEnd), textFg, bg);
            }
        }
    }
    buffer_.unlock();

    // Input line at the bottom.
    int inputRow = rc.bounds.h - 1;
    std::wstring promptText = ctx_.promptText();
    rc.screen.fillLine(rc.bounds.y + inputRow, rc.bounds.x, rc.bounds.w,
                       L' ', t.color(render::Role::CommandBar), t.color(render::Role::CommandBar));
    rc.screen.putText(rc.bounds.y + inputRow, rc.bounds.x, promptText,
                      t.color(render::Role::Accent), t.color(render::Role::CommandBar));
    std::wstring displayInput = input_;
    int promptLen = (int)promptText.size();
    if (promptLen + (int)displayInput.size() > rc.bounds.w)
    {
        int maxInputW = rc.bounds.w - promptLen;
        if (maxInputW > 0 && (int)displayInput.size() > maxInputW)
        {
            displayInput = displayInput.substr(displayInput.size() - maxInputW);
        }
    }
    rc.screen.putText(rc.bounds.y + inputRow, rc.bounds.x + promptLen,
                      displayInput, t.color(render::Role::CommandBarText),
                      t.color(render::Role::CommandBar));
    // Draw cursor.
    int cursorX = rc.bounds.x + promptLen + (int)inputCursor_;
    if (cursorX < rc.bounds.x + rc.bounds.w)
    {
        rc.screen.put(rc.bounds.y + inputRow, cursorX, L'\u2588',
                      t.color(render::Role::Accent), t.color(render::Role::CommandBar));
    }
}

bool ShellPane::onKey(const KeyEvent& key)
{
    if (exitRequested_)
    {
        return true;
    }

    // Ctrl+C: cancel current input.
    if (key.ctrl && (key.ch == L'c' || key.ch == L'C'))
    {
        input_.clear();
        inputCursor_ = 0;
        buffer_.append(ctx_.promptText() + input_);
        return true;
    }

    // Ctrl+L: clear buffer.
    if (key.ctrl && (key.ch == L'l' || key.ch == L'L'))
    {
        buffer_.clear();
        printPromptText();
        return true;
    }

    // Enter: execute command.
    if (key.key == Key::Enter)
    {
        std::wstring line = input_;
        input_.clear();
        inputCursor_ = 0;
        historySearchIdx_ = -1;
        savedInput_.clear();
        scrollOffset_ = 0;  // jump back to the most recent output
        executeCommand(line);
        return true;
    }

    // Backspace.
    if (key.key == Key::Backspace)
    {
        if (inputCursor_ > 0 && inputCursor_ <= input_.size())
        {
            input_.erase(inputCursor_ - 1, 1);
            --inputCursor_;
        }
        return true;
    }

    // Delete.
    if (key.key == Key::Delete)
    {
        if (inputCursor_ < input_.size())
        {
            input_.erase(inputCursor_, 1);
        }
        return true;
    }

    // Home.
    if (key.key == Key::Home || (key.ctrl && (key.ch == L'a' || key.ch == L'A')))
    {
        inputCursor_ = 0;
        return true;
    }

    // End.
    if (key.key == Key::End || (key.ctrl && (key.ch == L'e' || key.ch == L'E')))
    {
        inputCursor_ = input_.size();
        return true;
    }

    // Left/Right arrow.
    if (key.key == Key::Left)
    {
        if (inputCursor_ > 0)
        {
            --inputCursor_;
        }
        return true;
    }
    if (key.key == Key::Right)
    {
        if (inputCursor_ < input_.size())
        {
            ++inputCursor_;
        }
        return true;
    }

    // Up/Down arrow: history navigation.
    if (key.key == Key::Up)
    {
        if (historySearchIdx_ < 0)
        {
            savedInput_ = input_;
        }
        const auto& entries = ctx_.history().entries();
        if (!entries.empty())
        {
            int newIdx = historySearchIdx_ + 1;
            if (newIdx < (int)entries.size())
            {
                historySearchIdx_ = newIdx;
                input_ = entries[entries.size() - 1 - (size_t)historySearchIdx_];
                inputCursor_ = input_.size();
            }
        }
        return true;
    }
    if (key.key == Key::Down)
    {
        if (historySearchIdx_ > 0)
        {
            historySearchIdx_--;
            const auto& entries = ctx_.history().entries();
            input_ = entries[entries.size() - 1 - (size_t)historySearchIdx_];
            inputCursor_ = input_.size();
        }
        else if (historySearchIdx_ == 0)
        {
            historySearchIdx_ = -1;
            input_ = savedInput_;
            inputCursor_ = input_.size();
        }
        return true;
    }

    // Tab: autocomplete.
    if (key.key == Key::Tab)
    {
        auto aliases = ctx_.config().getAliases();
        std::vector<std::wstring> aliasNames;
        for (const auto& [name, _] : aliases)
        {
            aliasNames.push_back(name);
        }
        auto pathDirs = pathutils::getPathDirectories(ctx_.environment().getPath());

        // Determine whether we're completing a command or a path.
        std::wstring prefix = input_.substr(0, inputCursor_);
        bool isCommand = false;
        {
            std::wstring trimmed = stringutils::trim(prefix);
            if (trimmed.find_first_of(L" \t") == std::wstring::npos)
            {
                isCommand = true;
            }
        }

        std::vector<std::wstring> candidates;
        if (isCommand)
        {
            candidates = autocomplete_.completeCommand(prefix, ctx_.currentDirectory(),
                                                      pathDirs, aliasNames);
        }
        else
        {
            candidates = autocomplete_.completeFileOrDir(prefix, ctx_.currentDirectory());
        }

        if (!candidates.empty())
        {
            if (candidates.size() == 1)
            {
                // Find the last token in input_.
                size_t lastSpace = prefix.find_last_of(L" \t");
                std::wstring before = (lastSpace != std::wstring::npos)
                                          ? prefix.substr(0, lastSpace + 1)
                                          : L"";
                input_ = before + candidates[0];
                inputCursor_ = input_.size();
            }
            else
            {
                // Multiple candidates: show them in the buffer.
                std::wstring msg = L"  " + std::to_wstring(candidates.size()) + L" matches:";
                for (const auto& c : candidates)
                {
                    msg += L"  " + c;
                }
                buffer_.append(msg);
            }
        }
        return true;
    }

    // Escape.
    if (key.key == Key::Escape)
    {
        input_.clear();
        inputCursor_ = 0;
        return true;
    }

    // Printable character: insert.
    if (key.isPrint())
    {
        if (!input_.empty())
        {
            scrollOffset_ = 0;  // user is editing a command; show latest
        }
        input_.insert(input_.begin() + inputCursor_, key.ch);
        ++inputCursor_;
        // Typing replaces any prior mouse selection.
        selAnchorLine_ = selCursorLine_ = -1;
        selAnchorCol_ = selCursorCol_ = -1;
        return true;
    }

    return false;
}

void ShellPane::refresh()
{
    ctx_.jobs().reaper();
}

void ShellPane::onMouseWheel(int delta)
{
    if (delta > 0)
    {
        // Scroll up: show older content.
        ++scrollOffset_;
    }
    else if (delta < 0)
    {
        // Scroll down: show newer content.
        --scrollOffset_;
        if (scrollOffset_ < 0)
        {
            scrollOffset_ = 0;
        }
    }
}

void ShellPane::mapToBuffer(int rowInPane, int colInPane, int& lineIdx, int& colIdx)
{
    buffer_.lock();
    const auto& lines = buffer_.lines();
    int idx = firstVisible_ + rowInPane;
    if (idx < 0)
    {
        idx = 0;
    }
    if (idx >= (int)lines.size())
    {
        int n = (int)lines.size();
        buffer_.unlock();
        lineIdx = n;
        colIdx = 0;
        return;
    }
    int len = (int)lines[idx].size();
    if (colInPane < 0)
    {
        colInPane = 0;
    }
    if (colInPane > len)
    {
        colInPane = len;
    }
    buffer_.unlock();
    lineIdx = idx;
    colIdx = colInPane;
}

std::wstring ShellPane::selectedText()
{
    std::wstring result;
    if (selAnchorLine_ < 0 || selCursorLine_ < 0)
    {
        return result;
    }
    int l0 = std::min(selAnchorLine_, selCursorLine_);
    int l1 = std::max(selAnchorLine_, selCursorLine_);
    int c0 = (l0 == selAnchorLine_) ? selAnchorCol_ : selCursorCol_;
    int c1 = (l1 == selAnchorLine_) ? selAnchorCol_ : selCursorCol_;

    buffer_.lock();
    const auto& lines = buffer_.lines();
    for (int i = l0; i <= l1; ++i)
    {
        if (i >= (int)lines.size())
        {
            break;
        }
        const std::wstring& line = lines[i];
        int len = (int)line.size();
        std::wstring part;
        if (l0 == l1)
        {
            int cs = std::min(c0, len);
            int ce = std::min(c1, len);
            if (ce < cs)
            {
                std::swap(cs, ce);
            }
            part = line.substr(cs, ce - cs);
        }
        else if (i == l0)
        {
            int cs = std::min(c0, len);
            part = line.substr(cs);
        }
        else if (i == l1)
        {
            int ce = std::min(c1, len);
            part = line.substr(0, ce);
        }
        else
        {
            part = line;
        }
        result += part;
        if (i != l1)
        {
            result += L"\r\n";
        }
    }
    buffer_.unlock();
    return result;
}

void ShellPane::copyToClipboard(const std::wstring& text)
{
    if (text.empty())
    {
        return;
    }
    if (!::OpenClipboard(nullptr))
    {
        return;
    }
    ::EmptyClipboard();
    HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (mem)
    {
        wchar_t* dst = static_cast<wchar_t*>(::GlobalLock(mem));
        if (dst)
        {
            ::memcpy(dst, text.c_str(), text.size() * sizeof(wchar_t));
            dst[text.size()] = L'\0';
            ::GlobalUnlock(mem);
            ::SetClipboardData(CF_UNICODETEXT, mem);
        }
        else
        {
            ::GlobalFree(mem);
        }
    }
    ::CloseClipboard();
}

std::wstring ShellPane::clipboardText()
{
    std::wstring result;
    if (!::OpenClipboard(nullptr))
    {
        return result;
    }
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
        if (h)
        {
            const wchar_t* p = static_cast<const wchar_t*>(::GlobalLock(h));
            if (p)
            {
                result = p;
                ::GlobalUnlock(h);
            }
        }
    }
    ::CloseClipboard();
    return result;
}

void ShellPane::insertInput(const std::wstring& text)
{
    input_.insert(input_.begin() + (ptrdiff_t)inputCursor_, text.begin(), text.end());
    inputCursor_ += text.size();
    // Editing the input line supersedes any pending mouse selection.
    selAnchorLine_ = selCursorLine_ = -1;
    selAnchorCol_ = selCursorCol_ = -1;
}

void ShellPane::onMousePress(int rowInPane, int colInPane)
{
    int line = 0;
    int col = 0;
    mapToBuffer(rowInPane, colInPane, line, col);
    selecting_ = true;
    selAnchorLine_ = line;
    selAnchorCol_ = col;
    selCursorLine_ = line;
    selCursorCol_ = col;
}

void ShellPane::onMouseDrag(int rowInPane, int colInPane)
{
    if (!selecting_)
    {
        onMousePress(rowInPane, colInPane);
        return;
    }
    int line = 0;
    int col = 0;
    mapToBuffer(rowInPane, colInPane, line, col);
    selCursorLine_ = line;
    selCursorCol_ = col;
}

void ShellPane::onMouseRelease(int rowInPane, int colInPane)
{
    if (!selecting_)
    {
        return;
    }
    onMouseDrag(rowInPane, colInPane);
    selecting_ = false;  // keep anchor/cursor so the highlight stays visible
    if (selAnchorLine_ >= 0 && selCursorLine_ >= 0)
    {
        copyToClipboard(selectedText());
    }
}

void ShellPane::onMousePaste(int rowInPane, int colInPane)
{
    std::wstring text = clipboardText();
    if (text.empty())
    {
        return;
    }
    // If the click landed on the input line, place the cursor there first.
    if (rowInPane == paneH_ - 1)
    {
        int promptLen = (int)ctx_.promptText().size();
        int x = (int)input_.size();
        if (colInPane - promptLen > 0 && colInPane - promptLen < (int)input_.size())
        {
            x = colInPane - promptLen;
        }
        inputCursor_ = (size_t)x;
    }
    insertInput(text);
}

} // namespace kshell::ui
