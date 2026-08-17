cmake_minimum_required(VERSION 3.21)
project(aramf_gui VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(aramf
    src/core/ProjectModel.h
    src/core/Services.h
    src/ui/MainWindow.h
    src/ui/WorkflowWidget.h
    src/ui/pages/ProjectDetailsPage.h
    src/ui/pages/TemplatePage.h
    src/ui/pages/CategoryPage.h
    src/ui/pages/DevelopmentEnvironmentPage.h
    src/ui/pages/AiToolsPage.h
    src/ui/pages/ResourcesPage.h
    src/ui/pages/ReviewPage.h
    src/ui/pages/GeneratePage.h
    src/ui/pages/FinalizePage.h
    src/main.cpp
    src/core/ProjectModel.cpp
    src/core/Services.cpp
    src/ui/MainWindow.cpp
    src/ui/WorkflowWidget.cpp
    src/ui/pages/ProjectDetailsPage.cpp
    src/ui/pages/TemplatePage.cpp
    src/ui/pages/CategoryPage.cpp
    src/ui/pages/DevelopmentEnvironmentPage.cpp
    src/ui/pages/AiToolsPage.cpp
    src/ui/pages/ResourcesPage.cpp
    src/ui/pages/ReviewPage.cpp
    src/ui/pages/GeneratePage.cpp
    src/ui/pages/FinalizePage.cpp
    src/ui/pages/GuiFirstPages.h
    src/ui/pages/GuiFirstPages.cpp
    src/ui/pages/ProjectProfilePage.h
    src/ui/pages/ProjectProfilePage.cpp
)

target_include_directories(aramf PRIVATE src)
target_link_libraries(aramf PRIVATE Qt6::Widgets)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET aramf POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:aramf>"
            COMMENT "Deploying Qt runtime files" VERBATIM)
    endif()
endif()
