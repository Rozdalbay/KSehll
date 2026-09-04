# KShell Architecture

KShell is a native Windows command shell written in C++20. It does **not** delegate
command execution to `cmd.exe` or PowerShell — it implements its own lexer, parser,
process launcher, pipeline manager, job manager, and raw console keyboard handling.

All code lives in the `kshell` namespace. All strings are wide (`std::wstring`)
and all Windows APIs are called in their `W` (Unicode) forms via the
`UNICODE`/`_UNICODE` compile definitions.

## Module Overview

```
src/
├── main.cpp                   Entry point (wmain); TUI (default) or --legacy REPL.
├── core/
│   ├── Shell.h / .cpp         Classic REPL loop (used with --legacy).
│   ├── ShellContext.h/.cpp    Shared context: config, history, env, executor, output sink, themes.
│   └── ...
├── parser/
│   ├── Token.h                TokenType (Word, Pipe, Redirect, Background, ...).
│   ├── Lexer.h / .cpp         Character -> token conversion (quotes, escaping, variables).
│   ├── Parser.h / .cpp        Tokens -> Command / Pipeline structures.
│   └── Command.h              Command / Pipeline / RedirectSpec data structures.
├── execution/
│   ├── Process.h / .cpp       RAII ProcessHandle + CreateProcessW / wait / terminate.
│   ├── Redirection.h/.cpp     Redirection plan mapping for a command.
│   ├── CommandExecutor.h/.cpp Resolves and launches commands, pipelines, backgrounds.
│   └── Job.h                  Job state (ProcessHandles, pid, exit code, command).
├── jobs/
│   ├── Job.h / .cpp
│   └── JobManager.h / .cpp    Job registry, reap, kill, state updates.
├── terminal/
│   ├── Console.h / .cpp       Windows Console API color/output primitives.
│   ├── ConsoleSink.h          IOutputSink adapter wrapping Console for the REPL.
│   ├── IOutputSink.h          Output abstraction interface (virtual writeLine/write).
│   ├── Input.h / .cpp         Raw console keyboard input (ReadConsoleInputW), history nav, TAB.
│   └── Terminal.h / .cpp      Terminal initialization / shutdown.
├── builtin/
│   ├── BuiltinCommand.h       BuiltinEntry registry + function declarations.
│   ├── BuiltinRegistry.h/.cpp Unified command table (37+ commands).
│   ├── NewCommands.h/.cpp     New TUI-era builtins (open, tree, where, find, search, ...).
│   └── *.cpp                  One file per builtin group (cd, dir, echo, set, alias, ...).
├── history/
│   ├── History.h / .cpp       History list, dedupe, size cap, persistence.
├── config/
│   ├── Config.h / .cpp        Config file, colors, aliases, theme, font, TUI settings.
├── environment/
│   ├── Environment.h / .cpp   Env vars, $VAR / %VAR% expansion, environment block.
├── render/
│   ├── Color.h                RGB color type with ANSI conversion.
│   ├── Role.h                 Semantic theme roles (Background, Foreground, Accent, ...).
│   ├── Theme.h / .cpp         Theme: 24-role color palette with serialization.
│   ├── ThemeManager.h/.cpp    4 built-in themes, switch by index/name.
│   └── Screen.h / .cpp        Double-buffered virtual screen with cell diffing.
├── ui/
│   ├── Ui.h / .cpp            RenderContext, Pane base, draw helpers, OutputBuffer.
│   ├── Key.h                  KeyEvent / KeyType enum.
│   ├── Geometry.h             Rect / Size primitives with split/clamp.
│   ├── Fuzzy.h / .cpp         Subsequence matcher for fuzzy filtering.
│   ├── ExecEngine.h / .cpp    Pipe-capturing external command execution for TUI.
│   ├── ShellPane.h / .cpp     Interactive terminal pane (scrollback, buffer sink).
│   ├── CommandPalette.h/.cpp  Fuzzy search command palette overlay.
│   ├── Panels.h / .cpp        File/Process/System/Git/Jobs/History/Env panes.
│   └── App.h / .cpp           Main TUI event loop, console init, ANSI rendering.
├── process/
│   └── ProcessManager.h/.cpp  Windows process enumeration (Toolhelp32).
├── system/
│   └── SystemMonitor.h/.cpp   CPU/RAM/Disk metrics via Win32 APIs.
├── git/
│   └── Git.h / .cpp           Git integration via pipe capture.
├── autocomplete/ ...          TAB completion sources.
└── utils/
    ├── WinHandle.h            RAII wrapper for raw WINAPI HANDLEs (move-only).
    ├── StringUtils.h / .cpp   Trim, split, lowercase helpers.
    ├── PathUtils.h / .cpp     Path/file-name manipulation, home/appdata lookup.
    └── ErrorUtils.h / .cpp    Formatting of GetLastError() messages.
```

## Data Flow

### TUI Mode (default)

```text
stdin (ReadConsoleInputW)
   │  App::readInput() -> KeyEvent
   │  global shortcuts -> pane focus / tab switch / command palette
   ▼
ShellPane::onKey()
   │  builds input buffer, TAB autocomplete, Up/Down history, Ctrl+C
   ▼
ExecEngine::run(line)
   │  Lexer -> Parser -> CommandExecutor
   │  stdout/stderr captured via pipe -> OutputBuffer
   ▼
ShellPane::draw() -> Screen cell buffer
   ▼
App::present() -> ANSI escape sequences -> Win32 Console
```

### Legacy REPL Mode (--legacy)

```text
stdin (ReadConsoleInputW)
   │  Terminal/Input (keyboard, TAB complete, history Up/Down)
   ▼
Shell::repl() loop
   │  Lexer -> Parser -> CommandExecutor
   ▼
Console output -> Win32 Console API
```

## IOutputSink Abstraction

Both TUI and REPL share `ShellContext` and `CommandExecutor`. The difference
is the output path:

- **REPL**: `ShellContext.outputSink()` = `ConsoleSink` (wraps real Console API).
- **TUI**: `ShellContext.setOutputSink(BufferSink)` which appends to `OutputBuffer`.
  The TUI `ShellPane` reads from `OutputBuffer` and renders into the `Screen`.

This allows all 37+ builtins to work identically in both modes.

## Key Design Decisions

- **Wide strings throughout** (`std::wstring`) — correct Unicode handling on Windows.
- **RAII everywhere** — `WinHandle` and `ProcessHandle` are move-only; no raw leaked
  HANDLEs, no copy of non-copyable handles.
- **IOutputSink abstraction** — decouples output from rendering; enables both REPL
  and TUI modes with shared command infrastructure.
- **BuiltinRegistry** — single unified command table; new commands are registered
  in one place, legacy Shell builds its dispatch table from it.
- **Double-buffered Screen** — virtual grid of cells with diff-based present;
  only changed cells emit ANSI escapes for fast rendering.
- **Pipe-capturing ExecEngine** — external commands run with piped stdout/stderr,
  captured into OutputBuffer, then rendered into ShellPane.
- **`cmd.exe` only for batch files** — `.bat`/`.cmd` are launched via `cmd.exe /c`.
  All `.exe` and builtins run as first-class citizens.
- **Pipes** are implemented with `CreatePipe`, launched so each stage's stdio is
  wired to the pipe ends, then all stages are waited on.
- **Redirection** translates `>`, `>>`, `<`, `2>`, `2>>`, `2>&1` into
  `RedirectType` entries that become a `StartupInfo` handle table.
- **Jobs** are stored as `std::vector<Job>`; `Job` contains a
  `std::vector<ProcessHandle>` (non-copyable), so jobs are moved, never copied.
- **Custom key handling** (`ReadConsoleInputW`) gives Up/Down history, Ctrl+C
  (interrupts the running foreground job without closing the shell), Ctrl+L (clear),
  and TAB autocomplete.

## Compile-Time Settings (CMakeLists.txt)

- C++20, `-Wall -Wextra -Wpedantic` on MinGW; `/W4 /EHsc /utf-8` on MSVC.
- Defines: `UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN KSHELL_VERSION=L"1.0.0"`.
- Links: `kernel32 user32 shell32 advapi32 shlwapi psapi`.
- Targets: `KShell` (executable) + `kshell_core` (static lib) + test executables.

## Testing

Unit tests under `tests/` use a small macro harness (`test_framework.h`):

- `kshell_parser_tests` — lexer/parser tests.
- `kshell_history_tests` — history persistence/dedupe/cap tests.
- `kshell_command_tests` — string/path utility tests.
- `kshell_tui_tests` — fuzzy search, screen buffer, themes, geometry.

Run with `ctest --test-dir build --output-on-failure`.
