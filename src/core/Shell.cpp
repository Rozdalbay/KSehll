#include "core/Shell.h"
#include "builtin/BuiltinCommand.h"
#include "builtin/BuiltinRegistry.h"
#include "utils/StringUtils.h"
#include "utils/PathUtils.h"
#include "utils/ErrorUtils.h"
#include "parser/Lexer.h"
#include "parser/Parser.h"

#include <windows.h>

#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------
// Shell — классический REPL-режим KShell (запуск: KShell.exe --legacy).
//
// Отвечает за «жизненный цикл» шелла:
//   * initialize() — подготовка терминала, контекста и автодополнения;
//   * run()        — главный цикл «читаем команду -> выполняем -> повторяем»;
//   * processLine()— сквозная обработка одной строки: подстановка переменных,
//                    разворачивание алиасов, выполнение builtins и внешних
//                    конвейеров (CommandExecutor).
//
// TUI-режим (панель «Терминал») использует аналогичную логику в
// kshell::ui::ShellPane — держите поведение этих двух путей согласованным.
// ---------------------------------------------------------------------------

namespace kshell
{

namespace
{

// Обработчик Ctrl+C. Возвращаем TRUE, чтобы система НЕ завершала сам шелл:
// по умолчанию при нажатии Ctrl+C консольное приложение получает сигнал
// и может закрыться. Благодаря этому обработчику KShell переживает Ctrl+C,
// а дочерний процесс на переднем плане (разделяющий консоль) всё равно
// получает сигнал Ctrl+C и корректно прерывается. То есть Ctrl+C прерывает
// только выполняющуюся команду, но не сам шелл.
BOOL WINAPI ignoreCtrlC(DWORD ctrlType)
{
    // TRUE означает «мы обработали сигнал» — системный обработчик не
    // вызывается, и шелл не завершается. Дочерние процессы на переднем
    // плане, разделяющие консоль, всё равно получают этот сигнал.
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
    {
        return TRUE;
    }
    return FALSE;
}

// Собирает таблицу встроенных команд (builtins) один раз в виде вектора
// записей {имя, функция}. Таблица строится из единого реестра builtinRegistry(),
// чтобы и команды, и автодополнение использовали один источник истины.
std::vector<BuiltinEntry> builtinTable()
{
    std::vector<BuiltinEntry> entries;
    for (const auto& reg : builtinRegistry())
    {
        entries.push_back({reg.name.c_str(), reg.function});
    }
    return entries;
}

} // namespace

Shell::Shell()
{
}

Shell::~Shell()
{
    shutdown();
}

bool Shell::initialize()
{
    // Инициализация терминала: настраивает консоль, управляющие последов.
    // и цветовую палитру. Если терминал не готов, весь шелл запускать нельзя.
    if (!terminal_.initialize())
    {
        return false;
    }

    // Инициализация контекста: окружение, конфигурация, история, менеджер
    // задач, вывод и т.д. Все компоненты шелла живут внутри ShellContext.
    if (!ctx_.initialize())
    {
        return false;
    }

    // Передаём автодополнению список встроенных команд, чтобы по TAB
    // предлагались не только файлы/директории, но и имена builtins (help, cd...).
    std::vector<std::wstring> builtinNames;
    for (const auto& entry : builtinTable())
    {
        builtinNames.push_back(entry.name);
    }
    autocomplete_.setBuiltinNames(builtinNames);

    // Также собираем имена алиасов: они учитываются в автодополнении команд.
    const auto aliases = ctx_.config().getAliases();
    std::vector<std::wstring> aliasNames;
    for (const auto& [name, _] : aliases)
    {
        (void)_;
        aliasNames.push_back(name);
    }

    return true;
}

void Shell::shutdown()
{
    // Корректно завершаем работу: если выход не запрошен явно (например,
    // Ctrl+C нас не останавливал), сохраняем историю команд на диск.
    if (!ctx_.requestExit())
    {
        ctx_.history().saveToFile(ctx_.config().getHistoryFilePath());
    }
}

// Разворачивает алиасы в списке токенов команды. Например, alias ll="ls -l"
// превращает [ll, *.cpp] в [ls, -l, *.cpp]. Аргументы исходной команды
// дописываются В КОНЕЦ развёрнутой команды (поведение как в классических шеллах).
std::vector<std::wstring> Shell::resolveAliases(const std::vector<std::wstring>& tokens)
{
    if (tokens.empty())
    {
        return tokens;
    }

    const auto aliases = ctx_.config().getAliases();
    const auto it = aliases.find(tokens[0]);
    if (it == aliases.end())
    {
        // Первый токен не является алиасом — возвращаем команду как есть.
        return tokens;
    }

    const std::wstring aliasCmd = it->second;
    auto expanded = stringutils::splitCommandLine(aliasCmd);
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

// Расширяет переменные окружения в строке ввода: сначала «обновляет» копию
// окружения из системы (переменные могли измениться во время сессии), затем
// подставляет значения вида $VAR и %VAR% внутри строки.
bool Shell::expandVariables(std::wstring& input)
{
    ctx_.environment().refresh();
    input = ctx_.environment().expand(input);
    return true;
}

// Обработка одной строки, введённой пользователем. Это главная «точка входа»
// для логики шелла: строка проходит этапы ниже в строгом порядке.
Shell::ProcessInputResult Shell::processLine(const std::wstring& line)
{
    // 1) Обрезаем пробелы; пустая строка ничего не делает (новый промпт).
    const std::wstring trimmed = stringutils::trim(line);
    if (trimmed.empty())
    {
        return ProcessInputResult::Continue;
    }

    // 2) Подставляем переменные окружения ($VAR / %VAR%) ДО парсинга,
    //    чтобы значения с пробелами корректно разбивались на токены.
    std::wstring expanded = trimmed;
    expandVariables(expanded);

    // 3) Сохраняем команду в историю (как введено, без подстановок).
    ctx_.history().add(trimmed);

    // 4) Лексер + парсер превращают строку в конвейеры (пайплайны).
    //    Конвейер = до нескольких команд, соединённых символом '|'.
    std::optional<ParseError> error;
    Lexer lexer(expanded);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto pipelines = parser.parse(error);

    if (error)
    {
        ctx_.printError(L"Parse error: " + error->message);
        return ProcessInputResult::Continue;
    }

    // 5) Разворачиваем алиасы в каждой команде каждого конвейера.
    //    Делается ДО поиска builtins/внешних программ, чтобы алиас мог
    //    ссылаться на другую встроенную команду или внешний исполняемый файл.
    for (auto& pipeline : pipelines)
    {
        for (auto& command : pipeline.commands)
        {
            auto aliasTokens = resolveAliases(
                [&]() -> std::vector<std::wstring> {
                    std::vector<std::wstring> result;
                    result.push_back(command.program);
                    for (const auto& arg : command.arguments)
                    {
                        result.push_back(arg);
                    }
                    return result;
                }());

            if (!aliasTokens.empty())
            {
                command.program = aliasTokens[0];
                command.arguments.clear();
                for (size_t i = 1; i < aliasTokens.size(); ++i)
                {
                    command.arguments.push_back(aliasTokens[i]);
                }
            }
        }
    }

    // 6) Выполнение конвейеров по очереди.
    for (auto& pipeline : pipelines)
    {
        bool builtinHandled = false;

        // 6.1) Если это одиночная команда и она совпадает со встроенной —
        //      вызываем её напрямую (cd, dir, set, echo, ...).
        for (const auto& cmd : pipeline.commands)
        {
            BuiltinFunction builtinFunc = nullptr;
            for (const auto& entry : builtinTable())
            {
                if (stringutils::equalsIgnoreCase(cmd.program, entry.name))
                {
                    builtinFunc = entry.function;
                    break;
                }
            }

            if (builtinFunc && pipeline.commands.size() == 1)
            {
                std::vector<std::wstring> fullArgs;
                fullArgs.push_back(cmd.program);
                for (const auto& arg : cmd.arguments)
                {
                    fullArgs.push_back(arg);
                }
                auto result = builtinFunc(ctx_, fullArgs);
                if (result.exitRequested)
                {
                    // Например, exit/quit — просим шелл завершиться.
                    return ProcessInputResult::Exit;
                }
                // Ключевой момент: встроенная команда может вернуть
                // handled == false, чтобы ЗАПРОСИТЬ продолжение как внешняя
                // (так работает проброс `git` — см. builtinGit). В этом случае
                // мы НЕ считаем команду обработанной и не «проглатываем» её.
                if (result.handled)
                {
                    builtinHandled = true;
                }
            }
        }

        // 6.2) Если встроенная команда реально обработала запрос — переходим
        //      к следующему конвейеру. Если builtin вернул handled=false,
        //      идём дальше и пробуем запустить команду как внешнюю.
        if (builtinHandled)
        {
            continue;
        }

        // 6.3) Внешнее выполнение: обновляем исполнитель (CommandExecutor)
        //      под текущую рабочую директорию и запускаем конвейер через
        //      CreateProcessW (сам парсит '|', редиректы, фоновые задачи).
        ctx_.refreshExecutor();
        auto result = ctx_.executor().executePipeline(pipeline);

        if (result.succeeded && result.pid != 0 && !pipeline.background)
        {
            // Процесс успешно завершился — код выхода уже готов.
        }
        else if (!result.succeeded && !pipeline.background)
        {
            ctx_.printError(L"Command failed: " + pipeline.commands[0].program);
        }
    }

    return ProcessInputResult::Continue;
}

void Shell::printBanner()
{
    ctx_.console().setColor(ctx_.config().colors.promptColor);
    terminal_.printLine(L"KShell " KSHELL_VERSION);
    ctx_.console().resetColor();
    terminal_.printLine(L"Type \"help\" for available commands.");
    terminal_.printLine(L"");
}

// Главный цикл REPL (Read-Eval-Print Loop): читаем строку → выполняем → повторяем.
// Завершение цикла при exit/quit или когда requestExit() становится true.
int Shell::run()
{
    // Устанавливаем обработчик Ctrl+C (см. ignoreCtrlC выше): команду можно
    // прервать, шелл при этом останется жить.
    if (!::SetConsoleCtrlHandler(ignoreCtrlC, TRUE))
    {
        ctx_.printError(L"Failed to install console control handler");
    }

    printBanner();

    // Список встроенных команд для автодополнения внутри цикла.
    std::vector<std::wstring> builtinNames;
    for (const auto& entry : builtinTable())
    {
        builtinNames.push_back(entry.name);
    }

    // Основной цикл шелла.
    while (!ctx_.requestExit())
    {
        // Периодически «оживляем» состояние фоновых задач (завершились ли они).
        ctx_.executor().reapJobs();

        // Готовим данные для ввода: текущий промпт, каталоги из PATH,
        // имена алиасов и встроенных команд (для TAB-дополнения).
        const std::wstring prompt = ctx_.promptText();
        auto pathDirs = pathutils::getPathDirectories(ctx_.environment().getPath());

        const auto aliases = ctx_.config().getAliases();
        std::vector<std::wstring> aliasNames;
        for (const auto& [name, _] : aliases)
        {
            aliasNames.push_back(name);
        }

        // Читаем одну строку с поддержкой истории, автодополнения и отмены.
        auto inputResult = terminal_.input().readLine(
            prompt,
            ctx_.history(),
            autocomplete_,
            ctx_.config(),
            ctx_.currentDirectory(),
            pathDirs,
            aliasNames,
            builtinNames
        );

        // Пользователь ввёл exit/quit — выходим из цикла.
        if (inputResult.exitRequested)
        {
            break;
        }

        // Ctrl+C или Escape — просто переходим к следующей строке.
        if (inputResult.cancelled)
        {
            terminal_.printLine(L"");
            continue;
        }

        // Обрабатываем введённую строку.
        auto result = processLine(inputResult.line);
        if (result == ProcessInputResult::Exit)
        {
            break;
        }
    }

    // Снимаем обработчик Ctrl+C и сохраняем историю перед выходом.
    ::SetConsoleCtrlHandler(ignoreCtrlC, FALSE);
    ctx_.history().saveToFile(ctx_.config().getHistoryFilePath());
    return ctx_.exitCode();
}

} // namespace kshell