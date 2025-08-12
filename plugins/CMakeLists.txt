
function(configure_plugin plugin_name src_files)
    set(extra_args ${ARGN})
    list(LENGTH extra_args extra_args_count)

    set(extra_include_dirs "")
    set(extra_libs "")

    if(extra_args_count GREATER 0)
        list(GET extra_args 0 extra_include_dirs)
    endif()

    if(extra_args_count GREATER 1)
        list(GET extra_args 1 extra_libs)
    endif()

    add_library(${plugin_name} SHARED ${src_files})
    target_compile_definitions(${plugin_name} PRIVATE MCPSERVER_API_EXPORTS)

    set(base_include_dirs
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/core
        ${PROJECT_SOURCE_DIR}/include/sdk
    )

    target_link_libraries(${plugin_name} mcp_plugin_sdk mcp_core)

    if(extra_libs)
        target_link_libraries(${plugin_name} ${extra_libs})
    endif()

    if(WIN32)
        set_target_properties(${plugin_name} PROPERTIES SUFFIX ".dll")
    else()
        set_target_properties(${plugin_name} PROPERTIES SUFFIX ".so")
    endif()

    # automatically generate tools.json - always do this for runtime configs
    set(json_source_file "${CMAKE_CURRENT_SOURCE_DIR}/tools.json")
    set(json_target_file "${plugin_name}_tools.json")

    if(EXISTS ${json_source_file})
        # Create configs directory if not exists
        file(MAKE_DIRECTORY ${CONFIGS_OUTPUT_DIR})

        configure_file(
            ${json_source_file}
            ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            COPYONLY
        )

        # build when the plugin is built
        add_custom_command(
            OUTPUT ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${json_source_file}
            ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            DEPENDS ${json_source_file}
            COMMENT "Copying ${plugin_name} tools.json file to configs directory"
        )

        add_custom_target(${plugin_name}_json ALL
            DEPENDS ${CONFIGS_OUTPUT_DIR}/${json_target_file}
        )

        add_dependencies(${plugin_name} ${plugin_name}_json)

        # Install configs to bin/configs (always install configs)
        install(FILES ${json_source_file}
            DESTINATION bin/configs
            RENAME ${json_target_file}
            COMPONENT plugins
        )
    endif()

    # 安装插件动态库文件
    install(TARGETS ${plugin_name}
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION bin/plugins
        COMPONENT plugins
    )
endfunction()

# list all the directories and addsubdirectories to the project
function(add_subdirectories_to_project)
    # 获取当前目录下的所有条目（文件和目录）
    file(GLOB all_entries RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

    foreach(entry ${all_entries})
        # 拼接完整路径
        set(full_path "${CMAKE_CURRENT_SOURCE_DIR}/${entry}")

        # 仅处理存在的目录，且排除文件
        if(IS_DIRECTORY "${full_path}" AND NOT entry MATCHES "^\\.")
            # 排除 build 目录和 CMakeLists.txt（如果误判）
            if(NOT entry STREQUAL "build" AND NOT entry STREQUAL "CMakeLists.txt")
                add_subdirectory(${entry})
                message(STATUS "Added subdirectory: ${entry}")
            else()
                message(STATUS "Skipped directory: ${entry}")
            endif()
        else()
            # 打印被跳过的文件（方便调试）
            message(STATUS "Skipped non-directory: ${entry}")
        endif()
    endforeach()
endfunction()

add_subdirectories_to_project()