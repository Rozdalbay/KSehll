# KShell Commands Reference

KShell implements its own command language and does not require `cmd.exe` for
execution. Built-in commands run inside the shell process. Any other program is
launched via `CreateProcessW` after searching the current directory and `PATH`
(`.bat`/`.cmd` files are run via `cmd.exe /c`).

## Built-in Commands

### File & Directory
```
cd [path]            Change the current directory.  cd ..  cd \
pwd                  Print the current working directory.
dir [path]           List directory entries (Windows wide listing).
ls [path]            List directory entries (compact).
mkdir <path>         Create a directory (and parents).
rmdir <path>         Remove an empty directory.
touch <file>         Create an empty file or update its timestamp.
copy <src> <dst>     Copy a file.
move <src> <dst>     Move / rename a file.
del <file>           Delete a file.
open <path>          Open a file or directory with the system default application.
tree [path]          Display a directory tree.
```

### Text & Output
```
type <file>          Print a file's contents to the terminal.
cat <file>           Same as type.
echo <text>          Print text to the terminal (variables expanded).
clear / cls          Clear the terminal screen.
```

### Variables & Environment
```
set [NAME=value]     With no args, list all variables.  set NAME=value  sets one.
unset <name>         Remove an environment variable.
env                  List the shell's environment variables.
```

### Aliases & History
```
alias [name=value]   List aliases, or define one:  alias ll="ls -l"
unalias <name>       Remove an alias.
history              Show the command history.
```

### Jobs & Process
```
jobs                 List background jobs.
kill <jobid|pid>     Terminate a background job (kill %1) or process (kill 1234).
fg %<id>             Bring a background job to the foreground.
bg %<id>             Resume a suspended job in the background.
```

### System
```
which <program>      Show the full path used to launch a program.
where <program>      Locate instances of a program in PATH.
time                 Print the current time.
date                 Print the current date.
whoami               Print the current user name.
hostname             Print the machine name.
systeminfo           Print basic system information.
sysinfo              Display live CPU/RAM/Disk metrics.
proc                 List running processes (PID, name, CPU%, RAM).
top                  Display system resource usage in real-time.
```

### Search & Query
```
find <dir> <pattern> Find files matching a pattern recursively.
search <pattern>     Search for text in files in the current directory.
```

### TUI & Shell
```
help                 Show available commands and brief usage.
reload               Reload the configuration file.
theme <cmd>          Theme management: theme list, theme set <name>, theme get.
```

### Utilities
```
calc <expr>          Evaluate a mathematical expression.
json <text>          Pretty-print JSON text.
```

### Launching External Programs
```
git status           Run any external program via PATH search.
dir *.cpp | findstr cpp
program1 | program2 | program3
```

## Variables

- Refer to an environment variable with `$NAME` or `%NAME%`.
- Set with `set NAME=value`; remove with `unset NAME`.
- `echo $PATH` prints the value of `PATH`.

## PowerShell Cmdlet Emulation

KShell recognizes common PowerShell `Verb-Noun` cmdlets as built-in commands
(they run inside the shell, not via `powershell.exe`). Named parameters such as
`-Path`/`-Destination`/`-ItemType` are accepted and folded into positional
arguments:

```
Get-ChildItem [-Path <dir>]   same as dir / ls
Set-Location   [-Path <dir>]   same as cd  (~ maps to USERPROFILE)
Get-Location                    same as pwd
Get-Content    [-Path <file>]  same as type / cat
Get-Process                     same as proc (list processes)
Get-Date                        same as date
Get-Help                        same as help
Get-Command                     same as where
Write-Output                    same as echo
Clear-Host                      same as clear / cls
Copy-Item   [-Path <src> -Destination <dst>]   same as copy
Move-Item   [-Path <src> -Destination <dst>]   same as move
Remove-Item [-Path <file>]                    same as del
New-Item    [-Path <p> -ItemType <d|f>]         same as mkdir / touch
Get-ChildItem Env: -> env
Set-Env                         same as set NAME=value
Invoke-History                  same as history
Invoke-Job                      same as jobs
Get-Host / Get-ComputerInfo     same as sysinfo
```

Cmdlet names are matched case-insensitively, so `get-childitem` and
`GET-PROCESS` both work. These resolve as builtins in both the TUI and the
legacy REPL (`--legacy`).

## Pipes

```
dir | findstr cpp
type file.txt | findstr error
program1 | program2 | program3
```

Each stage is a separate process; output of one becomes the input of the next.

## Redirection

```
echo hello > file.txt      stdout -> file (overwrite)
echo hello >> file.txt     stdout -> file (append)
program < input.txt        stdin  <- file
program 2> errors.txt      stderr -> file
program > out.txt 2>&1     stdout and stderr -> same file
```

`2>` and `2>>` redirect standard error; `2>&1` merges stderr into stdout.

## Background Jobs

```
program &
```

The command runs in the background; the shell prints `[id] Running <command>`.
Use `jobs` to list, `kill %id` to terminate, `fg %id` to bring foreground,
`bg %id` to resume suspended.

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
alias ll="ls -l"            define
alias c="clear"             define
alias                       list
unalias ll                  remove
```

Aliases are persisted to the config file (`%APPDATA%\KShell\config`).

## Command History

Stored in `%USERPROFILE%\.kshell_history` (last 1000 non-empty commands).
Navigate with Up / Down, view with `history`.

## Autocomplete

Press TAB to complete file, directory, and executable names, plus built-in
command names.
