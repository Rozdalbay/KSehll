# KShell

[Русская версия](README_RU.md)

KShell is a native Windows command shell written in C++20. It implements its own
lexer, parser, process manager, pipeline manager, and key handling, without
relying on cmd.exe or PowerShell for command execution.

## Features

- Full-screen TUI environment with panels, tabs, and split panes (default mode)
- Command palette with fuzzy search (Ctrl+Shift+P)
- File browser, process manager, system monitor, git status, jobs, history, and env panels
- Full Git client: Overview, Changes + diff viewer + commit, branches, history, commit graph, remotes, and a GitHub tab (repos, pull requests, issues)
- 4 built-in themes (Default, Monokai, Dracula, Solarized) with live switching
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
- EN/RU language toggle with localization
- Variable tracking with change history
- Command tracing with filters
- Script execution (.bat, .ps1) with variable capture

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
| Ctrl+Shift+L | Toggle language EN/RU |
| Ctrl+Shift+T | New tab |
| Ctrl+Tab | Next tab |
| Ctrl+Shift+Tab | Previous tab |
| Ctrl+Shift+1..0 | Quick switch sidebar views |
| Ctrl+Shift+F | Focus file panel |
| Ctrl+Shift+M | Focus process panel |
| Ctrl+Shift+S | Focus system panel |
| Ctrl+Shift+G | Focus git panel |
| Ctrl+Shift+E | Focus env panel |
| Ctrl+Shift+V | Focus variables panel |
| Ctrl+Shift+T | Focus trace panel |
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

## Git / GitHub Client

Open the Git panel with `Ctrl+Shift+G`. It has 7 sections, switched with **Left / Right**
(or `[` / `]`):

| Section | Purpose |
|---------|---------|
| Overview | Repository status, current branch, ahead/behind, remote URL, last commit, quick actions |
| Changes | File list, diff viewer, staging, commit message fields, commit / commit & push |
| Branches | Create, rename, delete, merge, checkout branches |
| History | Commit list with detail pane; copy hash, branch from commit, revert, reset |
| Graph | ASCII commit graph |
| Remotes | Fetch, push, pull, add/remove remotes |
| GitHub | Connect with a personal access token, browse your repos, create pull requests and issues |

### Overview
| Key | Action |
|-----|--------|
| O | Open repository on GitHub (browser) |
| F | Fetch |
| P | Push |
| L | Pull |
| S | Stash changes |
| X | Pop stash |
| C | Clone repository |
| R / F5 | Refresh |

### Changes
| Key | Action |
|-----|--------|
| Enter / Space / S / U | Stage / unstage selected file |
| A | Stage all |
| Z | Unstage all |
| D | Discard selected file (confirm) |
| Tab | Switch between file list and commit message |
| C | Commit |
| P | Commit & push |
| X | Toggle unified / side-by-side diff |

### Branches
| Key | Action |
|-----|--------|
| Enter | Check out branch |
| N | Create branch |
| R | Rename branch |
| D | Delete branch (confirm) |
| M | Merge branch |
| F | Fetch all |

### History
| Key | Action |
|-----|--------|
| C | Copy full commit hash |
| B | Create branch from commit |
| V | Revert commit |
| R | Reset to commit |
| F5 | Reload history |

### Graph / Remotes

- **Graph**: Up/Down scroll, R / F5 reload.
- **Remotes**: F fetch, P push, L pull, A add remote, D remove (confirm), O open in browser.

### GitHub
| Key | Action |
|-----|--------|
| C | Connect / enter personal access token (stored in `%APPDATA%\KShell\github_token.txt`) |
| Up/Down | Choose repository |
| O | Open repository in browser |
| N | Create pull request |
| I | Create issue |
| R / F5 | Refresh repository list |

> **Note on Commit & Push.** "Push complete" means the local branch was pushed to the
> repository's configured `origin`. If nothing shows up on GitHub, check the Remote line in
> the Overview section (or run `git remote -v`): the local `origin` must point to the
> expected GitHub repository. When the repository has no remote and the GitHub tab is
> connected with a selected repository, `origin` is added automatically. Pushing from a
> detached HEAD is refused with a hint instead of silently reporting success.

## Command List

Built-ins: help, exit, quit, cd, pwd, dir, ls, mkdir, rmdir, touch, copy, move,
del, type, cat, echo, clear, cls, set, unset, env, alias, unalias, history,
jobs, kill, which, time, date, whoami, hostname, systeminfo,
open, tree, where, find, search, sysinfo, proc, top, calc, json, theme, reload,
fg, bg, trace, untrace, vars, clrtrace.

PowerShell aliases: Get-Process, Get-ChildItem, Get-Content, Set-Location,
New-Item, Remove-Item, Copy-Item, Move-Item, Get-History, Clear-Host, Write-Output,
Get-Date, Get-User, Get-Hostname, Get-SystemInfo, Get-Environment.

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

## Variable Tracking

```
set foo=bar
set foo=baz
vars
```

Variables are tracked with change history. Open the Variables panel (Ctrl+Shift+9) to view.

## Command Tracing

```
trace
dir
set x=1
untrace
```

Open the Trace panel (Ctrl+Shift+0) to view the log. Filters: F1 (all), F2 (commands), F3 (execution), F4 (variables), F5 (directories).

## Script Execution

```
test_script.bat
script.ps1
```

`.bat` and `.ps1` scripts are executed automatically. Variables set via `set X=Y` in the script are captured into tracking.

## Language Toggle

- Click `[EN]`/`[RU]` button in the title bar
- Keyboard shortcut: Ctrl+Shift+L

## Architecture

See docs/architecture.md, docs/commands.md, and docs/development.md for details.

## License

MIT. See LICENSE.
