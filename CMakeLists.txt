# CMakeLists.txt

cmake_minimum_required(VERSION 3.21)
project(aramf_gui VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(ARAMF_CORE_SOURCES
    src/core/AramfPaths.h
    src/core/ProjectMemory.h
    src/core/ProjectMemory.cpp
    src/core/ProjectModel.h
    src/core/ProjectModel.cpp
    src/core/Services.h
    src/core/Services.cpp
)

set(ARAMF_UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/WorkflowWidget.h
    src/ui/WorkflowWidget.cpp
    src/ui/pages/ProjectDetailsPage.h
    src/ui/pages/ProjectDetailsPage.cpp
    src/ui/pages/TemplatePage.h
    src/ui/pages/TemplatePage.cpp
    src/ui/pages/DevelopmentEnvironmentPage.h
    src/ui/pages/DevelopmentEnvironmentPage.cpp
    src/ui/pages/AiToolsPage.h
    src/ui/pages/AiToolsPage.cpp
    src/ui/pages/ResourcesPage.h
    src/ui/pages/ResourcesPage.cpp
    src/ui/pages/ReviewPage.h
    src/ui/pages/ReviewPage.cpp
    src/ui/pages/GeneratePage.h
    src/ui/pages/GeneratePage.cpp
    src/ui/pages/FinalizePage.h
    src/ui/pages/FinalizePage.cpp
    src/ui/pages/GuiFirstPages.h
    src/ui/pages/GuiFirstPages.cpp
)

add_executable(aramf
    src/main.cpp
    ${ARAMF_CORE_SOURCES}
    ${ARAMF_UI_SOURCES}
)

target_include_directories(aramf PRIVATE src)
target_link_libraries(aramf PRIVATE Qt6::Core Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    add_executable(aramf_core_tests
        tests/ProjectMemoryTests.cpp
        src/core/ProjectMemory.h
        src/core/ProjectMemory.cpp
        src/core/ProjectModel.h
        src/core/ProjectModel.cpp
        src/core/Services.h
        src/core/Services.cpp
    )
    target_include_directories(aramf_core_tests PRIVATE src)
    target_link_libraries(aramf_core_tests PRIVATE Qt6::Core)
    add_test(NAME aramf_core_tests COMMAND aramf_core_tests)
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET aramf POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:aramf>"
            COMMENT "Deploying Qt runtime files"
            VERBATIM)
    endif()
endif()
