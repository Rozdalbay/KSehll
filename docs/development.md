# KShell Development Guide

This document describes how to set up a development environment, build, test,
and extend KShell.

## Prerequisites

- Windows 10 or Windows 11 (x64)
- A C++20 compiler and CMake. Two toolchains are supported:
  - **Visual Studio 2022** with the "Desktop development with C++" workload (MSVC).
  - **MinGW-w64** (GCC 13+).
- CMake 3.20 or newer.
- (Optional) CTest, distributed with CMake, for the unit test suite.

## Toolchains

### Visual Studio 2022 (MSVC)

No separate compilers are required beyond the VS "Desktop development with C++"
workload. CMake picks up the compiler from the Visual Studio environment.

### MinGW-w64

Install a recent MinGW-w64 distribution (e.g. from MSYS2 or winlibs) and ensure
`g++`, `mingw32-make`, and `cmake` are on `PATH`.

> Note: On MinGW the executables are linked statically (`-static`), so
> `KShell.exe` and the test binaries do **not** require the MinGW runtime DLLs
> (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) at runtime.

## Building

### Configure

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Or use the presets:

```powershell
cmake -S . -B build --preset release
```

### Compile

```powershell
cmake --build build --config Release
```

The main executable is produced at:

```
build/KShell.exe        (MinGW / single-config generators)
build/Release/KShell.exe (Visual Studio multi-config generator)
```

## Testing

```powershell
cmake --build build --config Release
ctest --test-dir build --build-config Release -C Release --output-on-failure
```

The suite contains:

| Test binary             | Covers                                        |
|-------------------------|-----------------------------------------------|
| `kshell_parser_tests`   | Lexer (quotes, escaping, variables, operators) and parser (commands, pipelines, redirection, background, `;`) |
| `kshell_history_tests`  | History add, dedupe, empty suppression, size cap, persistence, clear |
| `kshell_command_tests`  | String and path utilities used by builtins    |

After a successful run you should see `100% tests passed`.

## Running

```powershell
build\KShell.exe
```

Useful non-interactive entry points:

```powershell
build\KShell.exe --version
build\KShell.exe --help
```

## Repository Layout

```
src/
├── main.cpp                 wmain; --version/--help; initializes and runs the Shell.
├── core/
│   ├── Shell               Main REPL loop, ctrl-handler, alias resolution, builtin dispatch.
│   └── ShellContext        Central state: config, history, env, prompt, cwd, executor, logger.
├── parser/
│   ├── Lexer               Character stream -> tokens (quotes, escaping, $VAR, %VAR%, operators).
│   ├── Parser              Tokens -> Pipeline/Command/RedirectSpec; reports ParseError.
│   └── Token / Command     Data structures.
├── execution/
│   ├── CommandExecutor     Resolves commands, launches single commands and pipelines, jobs.
│   ├── Process             RAII ProcessHandle; CreateProcessW / wait / terminate / pid query.
│   ├── Redirection         File-handle plans for <, >, >>, 2>, 2>>, 2>&1.
│   └── Pipeline            CreatePipe helper.
├── builtin/                One file per builtin group; all registered in BuiltinCommand.h.
├── jobs/                   Job state machine and JobManager.
├── history/                History list + persistence.
├── autocomplete/           TAB completion sources (files, dirs, exes, builtins, aliases).
├── terminal/               Console (color/output), Input (raw keyboard), Terminal wrapper.
├── environment/            Env vars, $VAR / %VAR% expansion, environment block.
├── config/                 Config file, aliases, colors, history size, log paths.
└── utils/                  WinHandle (RAII), Logger, StringUtils, PathUtils, ErrorUtils.

tests/                      Unit tests + test_framework.h (small macro harness).
docs/                       architecture.md, commands.md, development.md.
```

## Module Responsibilities

- **`Input`** reads raw console input events (`ReadConsoleInputW`) and implements
  left/right/home/end cursor movement, up/down history navigation, TAB
  completion, and Ctrl+C / Ctrl+D / Ctrl+L / Escape handling on the line buffer.
- **`Lexer`** converts a raw input string into a token vector. It understands
  double and single quotes, backslash escaping, `$VAR`, `%VAR%`, and the
  `| & ; < > >> 2> 2>> 2>&1` operators.
- **`Parser`** walks the token vector to build `Pipeline`/`Command` structures.
  Redirection operators are parsed into `RedirectSpec`s and a trailing `&`
  marks the pipeline as a background job.
- **`CommandExecutor`** resolves each command (current dir, then PATH; `.bat`/`.cmd`
  are run via `cmd.exe /c`) and either dispatches to a builtin or launches an
  external process via `CreateProcessW`.
- **`Redirection`** converts `RedirectSpec`s into open file handles. The handles
  are wired into `STARTUPINFO` and the shell keeps them alive until the child
  has been created.
- **`Process`** wraps `CreateProcessW`, `WaitForSingleObject`, `GetExitCodeProcess`,
  and `TerminateProcess`, holding `hProcess`/`hThread` in RAII `WinHandle`s.
- **`JobManager`** / **`CommandExecutor`** track background jobs with an internal
  id, Windows PID, command line, start time, and state (`Running`, `Finished`,
  `Failed`, `Terminated`).

## Key Design Decisions

- **Wide strings throughout** — `std::wstring` and the `W` variants of Windows
  APIs ensure correct Unicode handling (including Russian/Chinese paths).
- **RAII everywhere** — `WinHandle` (in `utils/WinHandle.h`) is move-only and
  closes on destruction; there are no raw leaked `HANDLE`s and no double closes.
- **Sorting of responsibilities** — Lexer and Parser are pure data-transformation
  components with no I/O; CommandExecutor is the only component that touches
  processes. This keeps the classes small and testable.
- **Signal handling** — `Shell::run()` installs a console control handler that
  swallows Ctrl+C so the shell itself never terminates; a foreground child that
  shares the console still receives the Ctrl+C and is interrupted, after which
  the shell resumes its prompt.
- **Logging** — `Logger` writes a timestamped, level-tagged stream to
  `%APPDATA%\KShell\logs\kshell.log` (append). Terminal output stays clean;
  logging never prints to the shell's stdout. Errors surfaced via
  `ShellContext::printError` are also logged.

## How to Add a Builtin

1. Add a declaration in `src/builtin/BuiltinCommand.h`.
2. Implement the handler in a new/appropriate `.cpp` under `src/builtin/`.
   The handler takes `ShellContext&` and `std::vector<std::wstring>` and returns
   a `BuiltinResult` with `exitCode`, `exitRequested`, and `handled`.
3. Register it in `builtinTable()` in `src/core/Shell.cpp`.
4. (Optional) Add it to `builtinNames()` in `WhichCommand.cpp` and to the
   `HelpCommand.cpp` listing.

## How to Add a Unit Test

Create a `.cpp` in `tests/` using `test_framework.h`, register the executable in
`tests/CMakeLists.txt` via `add_executable`, link `kshell_core`, and add an
`add_test` entry. Re-run `ctest --test-dir build`.

## Windows Console / Interaction Notes

- The shell runs in raw input mode during line editing (it clears
  `ENABLE_PROCESSED_INPUT` and uses `ReadConsoleInputW`), then restores the
  original console mode on exit.
- `--version` and `--help` short-circuits happen before the REPL, so they print
  cleanly to stdout and return `0`.
- Configuration and history are created automatically on first run under
  `%APPDATA%\KShell\` and `%USERPROFILE%\` respectively.

## Known Limitations

- `&&` and `||` are recognized by the lexer but are not yet executed; the parser
  reports "not supported yet".
- `.bat`/`.cmd` files are executed via `cmd.exe /c` (this is inherent to Windows
  shell scripts and is not used for normal executables).
- Ctrl+C is delivered to foreground children through console sharing; a child
  that installs its own ignore-handler will not be interrupted by it.
