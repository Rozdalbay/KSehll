cmake_minimum_required(VERSION 3.20)

project(KShellInstaller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    add_compile_options(/W4 /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The installer stub: a native Win32 console program that extracts its appended
# payload. The real KShell.exe is appended by the build helper (make_installer.ps1)
# to produce the final self-extracting KShell-Installer.exe.
add_executable(installer_stub main.cpp)
target_link_libraries(installer_stub PRIVATE shell32 ole32 shlwapi user32)

if(NOT MSVC)
    target_link_options(installer_stub PRIVATE -municode -static)
endif()

# Optional install rule: after building, use the helper to produce the final
# installer. The path to the built KShell.exe is passed via CMake cache:
#   cmake -S installer -B build-installer -DKSHELL_EXE=path/to/KShell.exe
add_custom_target(make_installer
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_CURRENT_SOURCE_DIR}/make_installer.ps1
            -SelfExe $<TARGET_FILE:installer_stub>
            -Payload ${KSHELL_EXE}
            -OutDir ${CMAKE_BINARY_DIR}
    DEPENDS installer_stub
    COMMENT "Assembling KShell-Installer.exe from embedded payload"
)
