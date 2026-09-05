cmake_minimum_required(VERSION 3.20)

project(KShell
    VERSION 1.0.0
    DESCRIPTION "KShell - a native Windows command shell written in C++20"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
                       KSHELL_VERSION=L"${PROJECT_VERSION}")

if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-function -Wno-unused-parameter)
endif()

set(KSHELL_CORE_SOURCES
    src/core/Shell.cpp
    src/core/ShellContext.cpp
    src/core/VariableTracker.cpp
    src/core/TraceLog.cpp
    src/core/Locale.cpp

    src/parser/Lexer.cpp
    src/parser/Parser.cpp

    src/execution/CommandExecutor.cpp
    src/execution/Process.cpp
    src/execution/Redirection.cpp

    src/builtin/CdCommand.cpp
    src/builtin/DirCommand.cpp
    src/builtin/ExitCommand.cpp
    src/builtin/EchoCommand.cpp
    src/builtin/SetCommand.cpp
    src/builtin/UnsetCommand.cpp
    src/builtin/AliasCommand.cpp
    src/builtin/HistoryCommand.cpp
    src/builtin/JobsCommand.cpp
    src/builtin/KillCommand.cpp
    src/builtin/ClearCommand.cpp
    src/builtin/WhichCommand.cpp
    src/builtin/HelpCommand.cpp
    src/builtin/NewCommands.cpp
    src/builtin/PSAliasCommands.cpp
    src/builtin/TraceCommand.cpp
    src/builtin/BuiltinRegistry.cpp

    src/jobs/JobManager.cpp

    src/history/History.cpp

    src/autocomplete/Autocomplete.cpp

    src/terminal/Terminal.cpp
    src/terminal/Input.cpp
    src/terminal/Console.cpp

    src/environment/Environment.cpp

    src/config/Config.cpp

    src/utils/StringUtils.cpp
    src/utils/PathUtils.cpp
    src/utils/ErrorUtils.cpp
    src/utils/Logger.cpp

    src/render/Theme.cpp
    src/render/ThemeManager.cpp
    src/render/Screen.cpp

    src/process/ProcessManager.cpp
    src/system/SystemMonitor.cpp
    src/git/Git.cpp

    src/ui/Ui.cpp
    src/ui/Fuzzy.cpp
    src/ui/ExecEngine.cpp
    src/ui/ShellPane.cpp
    src/ui/CommandPalette.cpp
    src/ui/Panels.cpp
    src/ui/App.cpp
)

add_library(kshell_core STATIC ${KSHELL_CORE_SOURCES})
target_include_directories(kshell_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(kshell_core PRIVATE kernel32 user32 shell32 advapi32 shlwapi psapi)

if(MSVC)
    target_compile_options(kshell_core PRIVATE /MP)
else()
    target_compile_options(kshell_core PRIVATE -finput-charset=UTF-8)
endif()

# Compile the Windows resource (application icon) with windres.
if(NOT MSVC)
    find_program(KSHELL_WINDRES windres)
    if(KSHELL_WINDRES)
        add_executable(KShell src/main.cpp resources/KShell.rc)
        target_link_libraries(KShell PRIVATE kshell_core)
        # windres only supports a limited set of options; forward the include
        # dir with -I so the relative ICON path inside the .rc resolves.
        set_source_files_properties(resources/KShell.rc PROPERTIES
            COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        add_executable(KShell src/main.cpp)
        target_link_libraries(KShell PRIVATE kshell_core)
    endif()
else()
    add_executable(KShell src/main.cpp resources/KShell.rc)
    target_link_libraries(KShell PRIVATE kshell_core)
endif()

if(NOT MSVC)
    target_link_options(KShell PRIVATE -municode -static)
endif()

enable_testing()
add_subdirectory(tests)
