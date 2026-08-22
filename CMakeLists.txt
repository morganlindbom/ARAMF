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
    src/core/ControlPlaneMigration.h
    src/core/ControlPlaneMigration.cpp
    src/core/ProjectMemory.h
    src/core/ProjectMemory.cpp
    src/core/FrameworkKnowledge.h
    src/core/FrameworkKnowledge.cpp
    src/core/EnvironmentCatalog.h
    src/core/EnvironmentCatalog.cpp
    src/core/AiCatalog.h
    src/core/AiCatalog.cpp
    src/core/RuleCatalog.h
    src/core/RuleCatalog.cpp
    src/core/MemoryCatalog.h
    src/core/MemoryCatalog.cpp
    src/core/ProjectPersistence.h
    src/core/ProjectPersistence.cpp
    src/core/ProjectModel.h
    src/core/ProjectModel.cpp
    src/core/Services.h
    src/core/Services.cpp
)

set(ARAMF_UI_SOURCES
    src/ui/mainwindow/MainWindow.h
    src/ui/mainwindow/MainWindow.cpp
    src/ui/workflow/WorkflowWidget.h
    src/ui/workflow/WorkflowWidget.cpp
    src/ui/workflow/WorkflowPageId.h
    src/ui/shared/PageSupport.h
    src/ui/shared/PageSupport.cpp
    src/ui/shared/CapabilityCheckGroup.h
    src/ui/shared/CapabilityCheckGroup.cpp
    src/ui/workflows/project/setup/ProjectSetupPage.h
    src/ui/workflows/project/setup/ProjectSetupPage.cpp
    src/ui/workflows/project/academic/ProjectAcademicPage.h
    src/ui/workflows/project/academic/ProjectAcademicPage.cpp
    src/ui/workflows/project/template/TemplateSelector.h
    src/ui/workflows/project/template/TemplateSelector.cpp
    src/ui/workflows/project/languages/ProjectLanguagesPage.h
    src/ui/workflows/project/languages/ProjectLanguagesPage.cpp
    src/ui/workflows/project/frameworks/ProjectFrameworksPage.h
    src/ui/workflows/project/frameworks/ProjectFrameworksPage.cpp
    src/ui/workflows/project/developmenttools/ProjectDevelopmentToolsPage.h
    src/ui/workflows/project/developmenttools/ProjectDevelopmentToolsPage.cpp
    src/ui/workflows/project/platforms/ProjectPlatformsPage.h
    src/ui/workflows/project/platforms/ProjectPlatformsPage.cpp
    src/ui/workflows/project/hardwarearchitecture/ProjectHardwareArchitecturePage.h
    src/ui/workflows/project/hardwarearchitecture/ProjectHardwareArchitecturePage.cpp
    src/ui/workflows/project/builddelivery/ProjectBuildDeliveryPage.h
    src/ui/workflows/project/builddelivery/ProjectBuildDeliveryPage.cpp
    src/ui/workflows/ai/agents/AiAgentsPage.h
    src/ui/workflows/ai/agents/AiAgentsPage.cpp
    src/ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h
    src/ui/workflows/ai/responsibilities/AiResponsibilitiesPage.cpp
    src/ui/workflows/ai/autonomy/AiAutonomyPage.h
    src/ui/workflows/ai/autonomy/AiAutonomyPage.cpp
    src/ui/workflows/ai/integration/AiIntegrationPage.h
    src/ui/workflows/ai/integration/AiIntegrationPage.cpp
    src/ui/workflows/resources/inventory/ResourceInventoryPage.h
    src/ui/workflows/resources/inventory/ResourceInventoryPage.cpp
    src/ui/workflows/resources/authority/ResourceAuthorityPage.h
    src/ui/workflows/resources/authority/ResourceAuthorityPage.cpp
    src/ui/workflows/resources/policy/ResourcePolicyPage.h
    src/ui/workflows/resources/policy/ResourcePolicyPage.cpp
    src/ui/workflows/rules/selection/RuleSelectionPage.h
    src/ui/workflows/rules/selection/RuleSelectionPage.cpp
    src/ui/workflows/rules/routing/RuleRoutingPage.h
    src/ui/workflows/rules/routing/RuleRoutingPage.cpp
    src/ui/workflows/memory/capture/MemoryCapturePage.h
    src/ui/workflows/memory/capture/MemoryCapturePage.cpp
    src/ui/workflows/memory/maintenance/MemoryMaintenancePage.h
    src/ui/workflows/memory/maintenance/MemoryMaintenancePage.cpp
    src/ui/workflows/output/review/ReviewPage.h
    src/ui/workflows/output/review/ReviewPage.cpp
    src/ui/workflows/output/generate/GeneratePage.h
    src/ui/workflows/output/generate/GeneratePage.cpp
    src/ui/workflows/output/verify/VerifyPage.h
    src/ui/workflows/output/verify/VerifyPage.cpp
    src/ui/workflows/output/finalize/FinalizePage.h
    src/ui/workflows/output/finalize/FinalizePage.cpp
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
        src/core/ControlPlaneMigration.h
        src/core/ControlPlaneMigration.cpp
        src/core/FrameworkKnowledge.h
        src/core/FrameworkKnowledge.cpp
        src/core/EnvironmentCatalog.h
        src/core/EnvironmentCatalog.cpp
        src/core/AiCatalog.h
        src/core/AiCatalog.cpp
        src/core/RuleCatalog.h
        src/core/RuleCatalog.cpp
        src/core/MemoryCatalog.h
        src/core/MemoryCatalog.cpp
        src/core/ProjectPersistence.h
        src/core/ProjectPersistence.cpp
        src/core/ProjectModel.h
        src/core/ProjectModel.cpp
        src/core/Services.h
        src/core/Services.cpp
    )
    target_include_directories(aramf_core_tests PRIVATE src)
    target_link_libraries(aramf_core_tests PRIVATE Qt6::Core)
    add_test(NAME aramf_core_tests COMMAND aramf_core_tests)

    add_executable(aramf_workflow_tests
        tests/WorkflowNavigationTests.cpp
        src/core/AramfPaths.h
        src/core/ProjectModel.h
        src/core/ProjectModel.cpp
        src/core/ProjectMemory.h
        src/core/ProjectMemory.cpp
        src/core/ControlPlaneMigration.h
        src/core/ControlPlaneMigration.cpp
        src/core/AiCatalog.h
        src/core/AiCatalog.cpp
        src/core/EnvironmentCatalog.h
        src/core/EnvironmentCatalog.cpp
        src/core/MemoryCatalog.h
        src/core/MemoryCatalog.cpp
        src/ui/shared/CapabilityCheckGroup.h
        src/ui/shared/CapabilityCheckGroup.cpp
        src/ui/workflows/resources/authority/ResourceAuthorityPage.h
        src/ui/workflows/resources/authority/ResourceAuthorityPage.cpp
        src/ui/workflows/ai/agents/AiAgentsPage.h
        src/ui/workflows/ai/agents/AiAgentsPage.cpp
        src/ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h
        src/ui/workflows/ai/responsibilities/AiResponsibilitiesPage.cpp
        src/ui/workflows/ai/autonomy/AiAutonomyPage.h
        src/ui/workflows/ai/autonomy/AiAutonomyPage.cpp
        src/ui/workflows/ai/integration/AiIntegrationPage.h
        src/ui/workflows/ai/integration/AiIntegrationPage.cpp
        src/ui/workflows/memory/maintenance/MemoryMaintenancePage.h
        src/ui/workflows/memory/maintenance/MemoryMaintenancePage.cpp
        src/ui/workflow/WorkflowWidget.h
        src/ui/workflow/WorkflowWidget.cpp
        src/ui/workflow/WorkflowPageId.h
    )
    target_include_directories(aramf_workflow_tests PRIVATE src)
    target_link_libraries(aramf_workflow_tests PRIVATE Qt6::Core Qt6::Widgets)
    add_test(NAME aramf_workflow_tests COMMAND aramf_workflow_tests)
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
