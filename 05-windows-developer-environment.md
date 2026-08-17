# Windows Developer Environment

## Historical toolchain paths

Recovered local environment:

```text
C++:   C:\msys64\ucrt64\bin\g++.exe
C:     C:\msys64\ucrt64\bin\gcc.exe
CMake: C:\Program Files\CMake\bin\cmake.exe
Ninja: C:\msys64\ucrt64\bin\ninja.exe
Qt:    C:\msys64\ucrt64\bin\qmake6.exe
```

## Runtime DLL deployment

Recovered exact CMake behavior:

```cmake
# Make the Windows build runnable outside the Qt/MinGW development shell.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET aramf POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations
                    "$<TARGET_FILE:aramf>"
            COMMENT "Deploying Qt runtime files"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt was not found; Qt runtime files will not be deployed")
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(MINGW_RUNTIME_DLL IN ITEMS
                libstdc++-6.dll
                libgcc_s_seh-1.dll
                libwinpthread-1.dll)
            add_custom_command(TARGET aramf POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${MINGW_BIN_DIR}/${MINGW_RUNTIME_DLL}"
                        "$<TARGET_FILE_DIR:aramf>"
                COMMENT "Deploying ${MINGW_RUNTIME_DLL}"
                VERBATIM
            )
        endforeach()
    endif()
endif()
```

This made the Windows build runnable without requiring Qt/MinGW on PATH at runtime.

## Developer convenience files

Recovered existence/behavior (exact content not fully recovered):

```text
CMakePresets.json
start-aramf.bat
.vscode/tasks.json
.vscode/launch.json
.vscode/c_cpp_properties.json
```

### Purpose

- `CMakePresets.json`: select/configure compiler/toolchain automatically.
- `start-aramf.bat`: double-click startup helper.
- `.vscode/tasks.json` + `launch.json`: F5 builds through CMake and launches the actual ARAMF executable; avoids VS Code's incorrect "build active file" action.
- `.vscode/c_cpp_properties.json`: IntelliSense points to Qt6 headers, `src`, `compile_commands.json`, and the MSYS2 compiler.

Historical launch configuration name: **Build and run ARAMF**.
