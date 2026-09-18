# CMakeLists.txt

cmake_minimum_required(VERSION 3.21)
project(aramf_gui VERSION 0.0.0 LANGUAGES CXX)

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
    src/core/GitIgnoreService.h
    src/core/GitIgnoreService.cpp
    src/core/ControlPlaneMigration.h
    src/core/ControlPlaneMigration.cpp
    src/core/ProjectMemory.h
    src/core/ProjectMemory.cpp
    src/core/ProjectMemoryCompaction.h
    src/core/ProjectMemoryCompaction.cpp
    src/core/CertificationService.h
    src/core/CertificationService.cpp
    src/core/MemoryCommand.h
    src/core/MemoryCommand.cpp
    src/core/ImprovementBacklog.h
    src/core/ImprovementBacklog.cpp
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
    src/core/ProjectSchema.h
    src/core/ProcessVersion.h
    src/core/ProcessVersion.cpp
    src/core/ConfigurationUpdateService.h
    src/core/ConfigurationUpdateService.cpp
    src/core/RuntimeOwnershipService.h
    src/core/RuntimeOwnershipService.cpp
    src/core/ExecutionOrchestrator.h
    src/core/ExecutionOrchestrator.cpp
    src/core/ComponentVersion.h
    src/core/ComponentVersion.cpp
    src/core/ProjectRootRebindService.h
    src/core/ProjectRootRebindService.cpp
    src/core/ProjectModel.h
    src/core/ProjectModel.cpp
    src/core/WorkflowPageMetadata.h
    src/core/WorkflowPageMetadata.cpp
    src/core/Services.h
    src/core/Services.cpp
    src/core/TemplateManager.cpp
    src/core/DocumentTemplate.h
    src/core/DocumentTemplate.cpp
    src/core/DocumentInstruction.h
    src/core/DocumentInstruction.cpp
    src/core/DocumentTemplateInspector.h
    src/core/DocumentTemplateInspector.cpp
    src/core/TemplateValidation.cpp
    src/core/ValidationRouting.h
    src/core/ValidationRouting.cpp
    src/core/WorkerContextResolver.h
    src/core/WorkerContextResolver.cpp
    src/core/ContextCoordinationService.h
    src/core/ContextCoordinationService.cpp
    src/core/UpdateService.h
    src/core/UpdateService.cpp
    src/core/AgentExecutionAdapter.h
    src/core/CodexExecutionAdapter.h
    src/core/CodexExecutionAdapter.cpp
    src/core/CodexExecutableResolver.h
    src/core/CodexExecutableResolver.cpp
    src/core/UpdateExecutionService.h
    src/core/UpdateExecutionService.cpp
    src/core/TaskSignature.h
    src/core/TaskSignature.cpp
    src/core/PredictiveOptimizationService.h
    src/core/PredictiveOptimizationService.cpp
    src/core/AdaptiveRoutingService.h
    src/core/AdaptiveRoutingService.cpp
    src/core/MemoryEvidenceFoundation.h
    src/core/MemoryEvidenceFoundation.cpp
    src/core/FoundationServices.h
    src/core/FoundationServices.cpp
)

set(ARAMF_UI_SOURCES
    src/ui/mainwindow/MainWindow.h
    src/ui/mainwindow/MainWindow.cpp
    src/ui/workflow/WorkflowWidget.h
    src/ui/workflow/WorkflowWidget.cpp
    src/ui/workflow/WorkflowPageId.h
    src/ui/shared/PageSupport.h
    src/ui/shared/PageSupport.cpp
    src/ui/shared/FooterProgressDisplay.h
    src/ui/shared/FooterProgressDisplay.cpp
    src/ui/shared/ParentOverview.h
    src/ui/shared/ParentOverview.cpp
    src/ui/workflows/release/overview/ReleaseOverviewPage.h
    src/ui/workflows/release/overview/ReleaseOverviewPage.cpp
    src/ui/workflows/release/productversion/ProductVersionPage.h
    src/ui/workflows/release/productversion/ProductVersionPage.cpp
    src/ui/workflows/release/componentversions/ComponentVersionsPage.h
    src/ui/workflows/release/componentversions/ComponentVersionsPage.cpp
    src/ui/workflows/release/schema/SchemaCompatibilityPage.h
    src/ui/workflows/release/schema/SchemaCompatibilityPage.cpp
    src/ui/workflows/release/readiness/ReleaseReadinessPage.h
    src/ui/workflows/release/readiness/ReleaseReadinessPage.cpp
    src/ui/workflows/release/history/ApprovalHistoryPage.h
    src/ui/workflows/release/history/ApprovalHistoryPage.cpp
    src/ui/shared/CapabilityCheckGroup.h
    src/ui/shared/CapabilityCheckGroup.cpp
    src/ui/workflows/project/setup/ProjectSetupPage.h
    src/ui/workflows/project/setup/ProjectSetupPage.cpp
    src/ui/workflows/project/overview/ProjectOverviewPage.h
    src/ui/workflows/project/overview/ProjectOverviewPage.cpp
    src/ui/workflows/project/modulestemplates/ProjectModulesTemplatesPage.h
    src/ui/workflows/project/modulestemplates/ProjectModulesTemplatesPage.cpp
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
    src/ui/workflows/update/review/FrameworkKnowledgeReviewPage.h
    src/ui/workflows/update/review/FrameworkKnowledgeReviewPage.cpp
    src/ui/workflows/update/apply/FrameworkKnowledgeApplyPage.h
    src/ui/workflows/update/apply/FrameworkKnowledgeApplyPage.cpp
    src/ui/workflows/update/backlog/ImprovementBacklogPage.h
    src/ui/workflows/update/backlog/ImprovementBacklogPage.cpp
    src/ui/workflows/update/configuration/UpdateConfigurationPage.h
    src/ui/workflows/update/configuration/UpdateConfigurationPage.cpp
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
        tests/WorkerTaskTests.cpp
        tests/ContextCoordinationTests.cpp
        tests/ProcessVersionTests.cpp
        tests/ProcessNamespaceMigrationTests.cpp
        tests/FoundationNamespaceTests.cpp
        tests/FoundationTests.cpp
        src/core/MemoryEvidenceFoundation.h
        src/core/MemoryEvidenceFoundation.cpp
        src/core/FoundationServices.h
        src/core/FoundationServices.cpp
        src/core/RuntimeOwnershipService.h
        src/core/RuntimeOwnershipService.cpp
        src/core/ExecutionOrchestrator.h
        src/core/ExecutionOrchestrator.cpp
        tests/P2ExecutionTests.cpp
        tests/P3PredictiveTests.cpp
        tests/P4RoutingTests.cpp
        src/core/AdaptiveRoutingService.h
        src/core/AdaptiveRoutingService.cpp
        src/core/TaskSignature.h
        src/core/TaskSignature.cpp
        src/core/PredictiveOptimizationService.h
        src/core/PredictiveOptimizationService.cpp
        src/core/GitIgnoreService.h
        src/core/GitIgnoreService.cpp
        src/core/ProjectMemory.h
        src/core/ProjectMemory.cpp
        src/core/ProjectMemoryCompaction.h
        src/core/ProjectMemoryCompaction.cpp
        src/core/CertificationService.h
        src/core/CertificationService.cpp
        src/core/MemoryCommand.h
        src/core/MemoryCommand.cpp
        src/core/ImprovementBacklog.h
        src/core/ImprovementBacklog.cpp
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
        src/core/FrameworkKnowledge.h
        src/core/FrameworkKnowledge.cpp
        src/core/ValidationRouting.h
        src/core/ValidationRouting.cpp
        src/core/WorkerContextResolver.h
        src/core/WorkerContextResolver.cpp
        src/core/ContextCoordinationService.h
        src/core/ContextCoordinationService.cpp
        src/core/UpdateService.h
        src/core/UpdateService.cpp
        src/core/AgentExecutionAdapter.h
        src/core/CodexExecutionAdapter.h
        src/core/CodexExecutionAdapter.cpp
        src/core/CodexExecutableResolver.h
        src/core/CodexExecutableResolver.cpp
        src/core/UpdateExecutionService.h
        src/core/UpdateExecutionService.cpp
        src/core/ProjectPersistence.h
        src/core/ProjectPersistence.cpp
        src/core/ProcessVersion.h
        src/core/ProcessVersion.cpp
        src/core/WorkflowPageMetadata.h
        src/core/WorkflowPageMetadata.cpp
        src/core/ComponentVersion.h
        src/core/ComponentVersion.cpp
        src/core/ProjectRootRebindService.h
        src/core/ProjectRootRebindService.cpp
        src/core/ProjectModel.h
        src/core/ProjectModel.cpp
        src/core/Services.h
        src/core/Services.cpp
        src/core/DocumentTemplate.h
        src/core/DocumentTemplate.cpp
        src/core/DocumentInstruction.h
        src/core/DocumentInstruction.cpp
        src/core/DocumentTemplateInspector.h
        src/core/DocumentTemplateInspector.cpp
    src/core/TemplateManager.cpp
    src/core/TemplateValidation.cpp
    )
    target_include_directories(aramf_core_tests PRIVATE src)
    target_link_libraries(aramf_core_tests PRIVATE Qt6::Core)
    add_dependencies(aramf_core_tests aramf)
    add_test(NAME aramf_core_tests COMMAND aramf_core_tests)

    add_executable(aramf_workflow_tests
        tests/WorkflowNavigationTests.cpp
        src/core/AramfPaths.h
        src/core/GitIgnoreService.h
        src/core/GitIgnoreService.cpp
        src/core/FrameworkKnowledge.h
        src/core/FrameworkKnowledge.cpp
        src/core/ProjectModel.h
        src/core/ProjectModel.cpp
        src/core/ProjectPersistence.h
        src/core/ProjectPersistence.cpp
        src/core/ProcessVersion.h
        src/core/ProcessVersion.cpp
        src/core/WorkflowPageMetadata.h
        src/core/WorkflowPageMetadata.cpp
        src/core/ComponentVersion.h
        src/core/ComponentVersion.cpp
        src/core/ProjectMemory.h
        src/core/ProjectMemory.cpp
        src/core/MemoryEvidenceFoundation.h
        src/core/MemoryEvidenceFoundation.cpp
        src/core/FoundationServices.h
        src/core/FoundationServices.cpp
        src/core/ProjectMemoryCompaction.h
        src/core/ProjectMemoryCompaction.cpp
        src/core/CertificationService.h
        src/core/CertificationService.cpp
        src/core/MemoryCommand.h
        src/core/MemoryCommand.cpp
        src/core/ImprovementBacklog.h
        src/core/ImprovementBacklog.cpp
        src/core/ControlPlaneMigration.h
        src/core/ControlPlaneMigration.cpp
        src/core/AiCatalog.h
        src/core/AiCatalog.cpp
        src/core/EnvironmentCatalog.h
        src/core/EnvironmentCatalog.cpp
        src/core/MemoryCatalog.h
        src/core/MemoryCatalog.cpp
        src/core/RuleCatalog.h
        src/core/RuleCatalog.cpp
        src/core/ValidationRouting.h
        src/core/ValidationRouting.cpp
        src/core/WorkerContextResolver.h
        src/core/WorkerContextResolver.cpp
        src/core/ContextCoordinationService.h
        src/core/ContextCoordinationService.cpp
        src/core/UpdateService.h
        src/core/UpdateService.cpp
        src/core/AgentExecutionAdapter.h
        src/core/CodexExecutionAdapter.h
        src/core/CodexExecutionAdapter.cpp
        src/core/CodexExecutableResolver.h
        src/core/CodexExecutableResolver.cpp
        src/core/UpdateExecutionService.h
        src/core/UpdateExecutionService.cpp
        src/core/Services.h
        src/core/Services.cpp
        src/core/DocumentTemplate.h
        src/core/DocumentTemplate.cpp
        src/core/DocumentInstruction.h
        src/core/DocumentInstruction.cpp
        src/core/DocumentTemplateInspector.h
        src/core/DocumentTemplateInspector.cpp
        src/core/TemplateManager.cpp
        src/core/TemplateValidation.cpp
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
        src/ui/workflows/update/review/FrameworkKnowledgeReviewPage.h
        src/ui/workflows/update/review/FrameworkKnowledgeReviewPage.cpp
        src/ui/workflows/update/apply/FrameworkKnowledgeApplyPage.h
        src/ui/workflows/update/apply/FrameworkKnowledgeApplyPage.cpp
        src/ui/workflows/update/backlog/ImprovementBacklogPage.h
        src/ui/workflows/update/backlog/ImprovementBacklogPage.cpp
        src/ui/workflow/WorkflowWidget.h
        src/ui/workflow/WorkflowWidget.cpp
        src/ui/workflow/WorkflowPageId.h
        src/ui/shared/ParentOverview.h
        src/ui/shared/ParentOverview.cpp
        src/ui/workflows/project/overview/ProjectOverviewPage.h
        src/ui/workflows/project/overview/ProjectOverviewPage.cpp
    )
    target_include_directories(aramf_workflow_tests PRIVATE src)
    target_link_libraries(aramf_workflow_tests PRIVATE Qt6::Core Qt6::Widgets)
    add_test(NAME aramf_workflow_tests COMMAND aramf_workflow_tests)

    add_executable(aramf_template_tests
        tests/TemplateConfigurationTests.cpp
        ${ARAMF_CORE_SOURCES}
        ${ARAMF_UI_SOURCES}
    )
    target_include_directories(aramf_template_tests PRIVATE src)
    target_link_libraries(aramf_template_tests PRIVATE Qt6::Core Qt6::Widgets)
    add_test(NAME aramf_template_tests COMMAND aramf_template_tests)
    set_tests_properties(aramf_template_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows;PATH=C:/msys64/ucrt64/bin")

    add_executable(aramf_update_campaign_runner
        test_update/runner/main.cpp
        ${ARAMF_CORE_SOURCES}
        src/ui/workflows/update/backlog/ImprovementBacklogPage.h
        src/ui/workflows/update/backlog/ImprovementBacklogPage.cpp
    )
    target_include_directories(aramf_update_campaign_runner PRIVATE src)
    target_link_libraries(aramf_update_campaign_runner PRIVATE Qt6::Core Qt6::Widgets)
    add_test(NAME aramf_update_campaign COMMAND aramf_update_campaign_runner)

    add_executable(aramf_configuration_update_tests
        tests/ConfigurationUpdateTests.cpp
        ${ARAMF_CORE_SOURCES})
    target_include_directories(aramf_configuration_update_tests PRIVATE src)
    target_link_libraries(aramf_configuration_update_tests PRIVATE Qt6::Core)
    add_test(NAME aramf_configuration_update COMMAND aramf_configuration_update_tests)
endif()

# Task policy is shared by every consumer of the existing core services.
foreach(ARAMF_TASK_TARGET aramf aramf_core_tests aramf_template_tests aramf_update_campaign_runner aramf_configuration_update_tests)
    if(TARGET ${ARAMF_TASK_TARGET})
        target_sources(${ARAMF_TASK_TARGET} PRIVATE src/core/WorkerTaskServices.h src/core/WorkerTaskServices.cpp)
    endif()
endforeach()

if(WIN32)
    # windeployqt installs Qt DLLs but does not deploy the MinGW runtime used
    # by this build.  Keep the application self-contained when launched from
    # the build directory (for example from Explorer or Android Studio).
    get_filename_component(ARAMF_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(ARAMF_RUNTIME_DLLS
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        libatomic-1.dll
        libssp-0.dll)
    function(aramf_deploy_compiler_runtime target)
        foreach(runtime_dll IN LISTS ARAMF_RUNTIME_DLLS)
            if(EXISTS "${ARAMF_COMPILER_BIN}/${runtime_dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${ARAMF_COMPILER_BIN}/${runtime_dll}"
                        "$<TARGET_FILE_DIR:${target}>/${runtime_dll}"
                    COMMENT "Deploying ${runtime_dll} for ${target}"
                    VERBATIM)
            endif()
        endforeach()
    endfunction()

    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET aramf POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:aramf>"
            COMMENT "Deploying Qt runtime files"
            VERBATIM)
        foreach(ARAMF_TEST_TARGET aramf_core_tests aramf_workflow_tests aramf_template_tests aramf_update_campaign_runner)
            if(TARGET ${ARAMF_TEST_TARGET})
                add_custom_command(TARGET ${ARAMF_TEST_TARGET} POST_BUILD
                    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:${ARAMF_TEST_TARGET}>"
                    COMMENT "Deploying Qt runtime files for ${ARAMF_TEST_TARGET}"
                    VERBATIM)
            endif()
        endforeach()
    endif()
    aramf_deploy_compiler_runtime(aramf)
    foreach(ARAMF_TEST_TARGET aramf_core_tests aramf_workflow_tests aramf_template_tests aramf_update_campaign_runner)
        if(TARGET ${ARAMF_TEST_TARGET})
            aramf_deploy_compiler_runtime(${ARAMF_TEST_TARGET})
        endif()
    endforeach()
endif()
