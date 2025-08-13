cmake_minimum_required(VERSION 3.10)
project(PluginServer VERSION 0.1.0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set plugin output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Set config file output directory
set(CONFIGS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/configs)

include_directories(include)
include_directories(include/sdk)
include_directories(src)

# msvc utf-8
if(MSVC)
    add_compile_options(/utf-8)

    # nominmax
    add_definitions(-DNOMINMAX)
endif()

# Add source files
set(SOURCES
    main.cc
    src/plugin_manager.cpp
)

add_executable(PluginServer ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(PluginServer Threads::Threads)

# Add subdirectories
add_subdirectory(protocol)
add_subdirectory(include/core)
add_subdirectory(include/sdk)
add_subdirectory(plugins)
add_subdirectory(tools)

# Link libraries
target_link_libraries(PluginServer mcp_core mcp_plugin_sdk)

if(WIN32)
    target_link_libraries(PluginServer wsock32 ws2_32)
endif()

install(TARGETS PluginServer
    RUNTIME DESTINATION bin
    COMPONENT server
)

install(TARGETS plugin_ctl
    RUNTIME DESTINATION bin
    COMPONENT server
)

# Install config directory
install(DIRECTORY ${CONFIGS_OUTPUT_DIR}/
    DESTINATION bin/configs
    COMPONENT plugins
    OPTIONAL
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPackConfig.cmake)
include(CPack)