# KShell

KShell is a native Windows command shell written in C++20. It implements its own
lexer, parser, process manager, pipeline manager, and key handling, without
relying on cmd.exe or PowerShell for command execution.

## Features

- Full-screen TUI environment with panels, tabs, and split panes (default mode)
- Command palette with fuzzy search (Ctrl+Shift+P)
- File browser, process manager, system monitor, git status, jobs, history, and env panels
- 4 built-in themes (Dark, Light, High Contrast, Monochrome) with live switching
- Interactive shell pane with scrollback and buffer sink
- Custom command-line lexer and parser (quotes, escaping, variables, pipes, redirection)
- Native process execution via `CreateProcessW`
- Pipes (command1 | command2)
- Redirection (<, >, >>, 2>, 2>>, 2>&1)
- Background jobs (program &)
- Environment variable management (set, unset, env, $VAR, %VAR%)
- Aliases (alias, unalias) persisted to config
- Command history with up/down navigation, persisted between runs
- TAB autocomplete (files, directories, executables, builtins)
- 37+ built-in commands
- Ctrl+C handling (does not close the shell)
- Unicode (wide) string handling throughout
- Configurable themes and color scheme
- RAII-based Windows HANDLE management
- Logger to %APPDATA%\KShell\logs\kshell.log
- CMake + CTest test suite (55+ tests)

## Requirements

- Windows 10 or Windows 11 (x64)
- MinGW-w64 GCC 14.2+ (primary) or MSVC 2022
- CMake 3.20 or newer

## Building

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

The resulting executable is located at `build\KShell.exe`.

## Running Tests

```powershell
ctest --test-dir build --output-on-failure
```

## Running

```
KShell.exe
```

Default mode is the TUI environment. Use `--legacy` for the classic REPL:

```
KShell.exe --legacy
KShell.exe --version
KShell.exe --help
```

## Keyboard Shortcuts

### Global
| Key | Action |
|-----|--------|
| Ctrl+Shift+P | Open command palette |
| Ctrl+Shift+T | New tab |
| Ctrl+Tab | Next tab |
| Ctrl+Shift+Tab | Previous tab |
| Ctrl+Shift+F | Focus file panel |
| Ctrl+Shift+M | Focus process panel |
| Ctrl+Shift+S | Focus system panel |
| Ctrl+Shift+G | Focus git panel |
| Ctrl+Shift+E | Focus env panel |
| Ctrl+Shift+J | Focus jobs panel |
| Ctrl+Shift+H | Focus history panel |
| Ctrl+Shift+W | Focus shell pane |
| Ctrl+R | Reverse history search |

### Shell Pane
| Key | Action |
|-----|--------|
| Enter | Execute command |
| Up/Down | History navigation |
| Tab | Autocomplete |
| Ctrl+C | Cancel running command |
| Ctrl+L | Clear screen |

## Command List

Built-ins: help, exit, quit, cd, pwd, dir, ls, mkdir, rmdir, touch, copy, move,
del, type, cat, echo, clear, cls, set, unset, env, alias, unalias, history,
jobs, kill, which, time, date, whoami, hostname, systeminfo,
open, tree, where, find, search, sysinfo, proc, top, calc, json, theme, reload,
fg, bg.

External programs are launched via CreateProcessW after searching the current
directory and PATH.

## Pipes

```
dir | findstr cpp
type file.txt | findstr error
program1 | program2 | program3
```

## Redirection

```
echo hello > file.txt
echo hello >> file.txt
program < input.txt
program 2> errors.txt
program > output.txt 2>&1
```

## Background Jobs

```
python server.py &
[1] Running python server.py
jobs
kill %1
fg %1
bg %1
```

## Aliases

```
alias ll="ls -l"
alias c="clear"
alias proj="cd C:\Projects"
unalias ll
```

## History

Stored in `%USERPROFILE%\.kshell_history` (last 1000 non-empty commands).
Navigate with Up / Down, view with `history`.

## Autocomplete

Press TAB to complete file, directory, and executable names as well as
built-in command names.

## Themes

Switch themes live from the command palette or with:

```
theme list
theme set Dark
theme set Light
theme set "High Contrast"
theme set Monochrome
```

## Configuration

Config file: `%APPDATA%\KShell\config.ini`. Created automatically on first run.
Contains prompt, aliases, history size, theme, font, and TUI settings.

## Architecture

See docs/architecture.md, docs/commands.md, and docs/development.md for details.

## License

MIT. See LICENSE.
