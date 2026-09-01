cmake_minimum_required(VERSION 3.21)
project(aramf_course_sot_acceptance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
add_executable(aramf_course_sot_acceptance
    acceptance.cpp
    ${ROOT}/src/core/ControlPlaneMigration.cpp
    ${ROOT}/src/core/ProjectMemory.cpp
    ${ROOT}/src/core/ProjectMemoryCompaction.cpp
    ${ROOT}/src/core/CertificationService.cpp
    ${ROOT}/src/core/MemoryCommand.cpp
    ${ROOT}/src/core/ImprovementBacklog.cpp
    ${ROOT}/src/core/FrameworkKnowledge.cpp
    ${ROOT}/src/core/EnvironmentCatalog.cpp
    ${ROOT}/src/core/AiCatalog.cpp
    ${ROOT}/src/core/RuleCatalog.cpp
    ${ROOT}/src/core/MemoryCatalog.cpp
    ${ROOT}/src/core/ProjectPersistence.cpp
    ${ROOT}/src/core/ProjectRootRebindService.cpp
    ${ROOT}/src/core/ProjectModel.cpp
    ${ROOT}/src/core/Services.cpp
    ${ROOT}/src/core/ValidationRouting.cpp
    ${ROOT}/src/core/UpdateService.cpp
    ${ROOT}/src/core/AgentExecutionAdapter.h
    ${ROOT}/src/core/CodexExecutionAdapter.cpp
    ${ROOT}/src/core/CodexExecutableResolver.cpp
    ${ROOT}/src/core/UpdateExecutionService.cpp)
target_include_directories(aramf_course_sot_acceptance PRIVATE ${ROOT}/src)
target_link_libraries(aramf_course_sot_acceptance PRIVATE Qt6::Core)
