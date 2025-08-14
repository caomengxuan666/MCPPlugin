add_executable(plugin_ctl plugin_ctl.cpp)

if(NOT MSVC)
    target_link_libraries(plugin_ctl stdc++fs)
endif()

# Install plugin_ctl to bin directory
install(TARGETS plugin_ctl
    RUNTIME DESTINATION bin
    COMPONENT server
)