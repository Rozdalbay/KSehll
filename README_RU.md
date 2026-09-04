# KShell

Полноэкранный TUI-шелл для Windows, написанный на C++20. Реализует собственный лексер, парсер, менеджер процессов и пайпов — не зависит от cmd.exe или PowerShell для выполнения команд.

## Возможности

- Полноэкранное TUI-окружение с панелями, вкладками и сплит-панами (режим по умолчанию)
- Палитра команд с нечётким поиском (Ctrl+Shift+P)
- Файловый менеджер, менеджер процессов, монитор системы, git-статус, задачи, история, переменные окружения
- 4 встроенных темы (Default, Monokai, Dracula, Solarized) с переключением на лету
- Интерактивная панель терминала с прокруткой и буфером вывода
- Собственный лексер и парсер (кавычки, экранирование, переменные, пайпы, редиректы)
- Запуск внешних процессов через `CreateProcessW`
- Пайпы (команда1 | команда2)
- Редиректы (<, >, >>, 2>, 2>>, 2>&1)
- Фоновые задачи (программа &)
- Управление переменными окружения (set, unset, env, $VAR, %VAR%)
- Алиасы (alias, unalias), сохраняются в конфиг
- История команд с навигацией вверх/вниз, сохраняется между запусками
- Автодополнение по TAB (файлы, директории, исполняемые файлы, встроенные команды)
- 37+ встроенных команд
- Обработка Ctrl+C (не закрывает шелл)
- Полная поддержка Unicode (широкие строки)
- Настраиваемые темы и цветовые схемы
- RAII-управление Windows HANDLE
- Логирование в `%APPDATA%\KShell\logs\kshell.log`
- Тесты CMake + CTest (55+ тестов)
- Переключатель языка EN/RU
- Отслеживание переменных с историей изменений
- Трассировка выполнения команд
- Запуск скриптов (.bat, .ps1) с захватом переменных

## Системные требования

- Windows 10 или Windows 11 (x64)
- MinGW-w64 GCC 14.2+ (основной) или MSVC 2022
- CMake 3.20 или новее

## Сборка

```powershell
$env:PATH="C:\mingw64\bin;"+$env:PATH
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

Исполняемый файл: `build\KShell.exe`

## Запуск тестов

```powershell
ctest --test-dir build --output-on-failure
```

## Запуск

```
KShell.exe
```

Режим по умолчанию — TUI-окружение. Классический REPL:

```
KShell.exe --legacy
KShell.exe --version
KShell.exe --help
```

## Горячие клавиши

### Глобальные

| Клавиша | Действие |
|---------|----------|
| Ctrl+Shift+P | Палитра команд |
| Ctrl+Shift+L | Переключение языка EN/RU |
| Ctrl+Tab | Следующая вкладка |
| Ctrl+Shift+Tab | Предыдущая вкладка |
| Ctrl+Shift+1..0 | Быстрое переключение панелей |
| Ctrl+Shift+F | Файловый менеджер |
| Ctrl+Shift+M | Процессы |
| Ctrl+Shift+S | Система |
| Ctrl+Shift+G | Git |
| Ctrl+Shift+E | Переменные окружения |
| Ctrl+Shift+V | Отслеживаемые переменные |
| Ctrl+Shift+T | Трассировка |
| Ctrl+Shift+J | Задачи |
| Ctrl+Shift+H | История |
| Ctrl+R | Обратный поиск истории |

### Панель терминала

| Клавиша | Действие |
|---------|----------|
| Enter | Выполнить команду |
| Вверх/Вниз | Навигация по истории |
| Tab | Автодополнение |
| Ctrl+C | Отмена выполняющейся команды |
| Ctrl+L | Очистить экран |

## Встроенные команды

help, exit, quit, cd, pwd, dir, ls, mkdir, rmdir, touch, copy, move,
del, type, cat, echo, clear, cls, set, unset, env, alias, unalias, history,
jobs, kill, which, time, date, whoami, hostname, systeminfo,
open, tree, where, find, search, sysinfo, proc, top, calc, json, theme, reload,
fg, bg, trace, untrace, vars, clrtrace.

Алиасы PowerShell-команд: Get-Process, Get-ChildItem, Get-Content, Set-Location,
New-Item, Remove-Item, Copy-Item, Move-Item, Get-History, Clear-Host, Write-Output,
Get-Date, Get-User, Get-Hostname, Get-SystemInfo, Get-Environment.

## Пайпы

```bash
dir | findstr cpp
type file.txt | findstr error
program1 | program2
```

## Редиректы

```bash
echo hello > file.txt
echo hello >> file.txt
program < input.txt
program 2> errors.txt
program > output.txt 2>&1
```

## Фоновые задачи

```bash
python server.py &
jobs
kill %1
fg %1
bg %1
```

## Алиасы

```bash
alias ll="ls -l"
alias c="clear"
unalias ll
```

## История

Сохраняется в `%USERPROFILE%\.kshell_history` (последние 1000 команд).
Навигация: вверх/вниз, просмотр: `history`.

## Автодополнение

Нажмите TAB для дополнения имён файлов, директорий, исполняемых файлов и встроенных команд.

## Темы

```bash
theme list
theme set Dark
theme set Light
theme set "High Contrast"
theme set Monochrome
```

## Отслеживание переменных

```bash
set foo=bar
set foo=baz
vars
```

Переменные отслеживаются с историей изменений. Откройте панель «Переменные» (Ctrl+Shift+9) для просмотра.

## Трассировка команд

```bash
trace
dir
set x=1
untrace
```

Откройте панель «Трассировка» (Ctrl+Shift+0) для просмотра лога. Фильтры: F1 (всё), F2 (команды), F3 (выполнение), F4 (переменные), F5 (каталоги).

## Запуск скриптов

```bash
test_script.bat
script.ps1
```

Скрипты `.bat` и `.ps1` запускаются автоматически. Переменные, заданные через `set X=Y` в скрипте, попадают в отслеживание.

## Переключение языка

- Кнопка `[EN]`/`[RU]` в заголовке окна (клик мышкой)
- Горячая клавиша: Ctrl+Shift+L

## Конфигурация

Файл конфигурации: `%APPDATA%\KShell\config.ini`. Создаётся автоматически при первом запуске. Содержит промпт, алиасы, размер истории, тему, шрифт и настройки TUI.

## Архитектура

Подробности: docs/architecture.md, docs/commands.md, docs/development.md.

## Лицензия

MIT. См. LICENSE.
