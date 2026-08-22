cmake_minimum_required(VERSION 3.21)
project(aramf_campaign_runner LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core)
add_executable(aramf_campaign_runner
    main.cpp
    ../../src/core/AramfPaths.h
    ../../src/core/ProjectMemory.h ../../src/core/ProjectMemory.cpp
    ../../src/core/FrameworkKnowledge.h ../../src/core/FrameworkKnowledge.cpp
    ../../src/core/EnvironmentCatalog.h ../../src/core/EnvironmentCatalog.cpp
    ../../src/core/AiCatalog.h ../../src/core/AiCatalog.cpp
    ../../src/core/RuleCatalog.h ../../src/core/RuleCatalog.cpp
    ../../src/core/MemoryCatalog.h ../../src/core/MemoryCatalog.cpp
    ../../src/core/ProjectPersistence.h ../../src/core/ProjectPersistence.cpp
    ../../src/core/ProjectModel.h ../../src/core/ProjectModel.cpp
    ../../src/core/Services.h ../../src/core/Services.cpp)
target_include_directories(aramf_campaign_runner PRIVATE ../../src)
target_link_libraries(aramf_campaign_runner PRIVATE Qt6::Core)
