add_executable(plugin_ctl plugin_ctl.cpp)

# After build copy the plugin_ctl executable to the plugin directory
add_custom_command(TARGET plugin_ctl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:plugin_ctl>
    ${PROJECT_SOURCE_DIR}/plugins/
    COMMAND_EXPAND_LISTS
)

# 安装plugin_ctl到bin目录
install(TARGETS plugin_ctl
    RUNTIME DESTINATION bin
    COMPONENT server
)