cmake_minimum_required(VERSION 3.10)
project(PluginServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(include/sdk)
include_directories(src)

# msvc utf-8
if(MSVC)
    add_compile_options(/utf-8)

    # nominmax
    add_definitions(-DNOMINMAX)
endif()

# 添加源文件
set(SOURCES
    main.cc
    src/plugin_manager.cpp
)

add_executable(PluginServer ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(PluginServer Threads::Threads mcp_core mcp_plugin_sdk)

if(WIN32)
    target_link_libraries(PluginServer wsock32 ws2_32)
endif()

add_subdirectory(protocol)
add_subdirectory(include/core)
add_subdirectory(include/sdk)
add_subdirectory(plugins)
add_subdirectory(tools)

install(TARGETS PluginServer
    RUNTIME DESTINATION bin
    COMPONENT server
)

install(TARGETS plugin_ctl
    RUNTIME DESTINATION bin
    COMPONENT server
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPackConfig.cmake)
include(CPack)