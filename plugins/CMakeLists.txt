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
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/configs)

        # Use file(COPY) with RENAME for better cross-platform compatibility
        add_custom_command(TARGET ${plugin_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${json_source_file}
            ${CMAKE_BINARY_DIR}/configs/${json_target_file}
            COMMENT "Copying ${plugin_name} tools.json file to configs directory"
        )

        # Install configs to bin/configs (always install configs)
        install(FILES ${json_source_file}
            DESTINATION bin/configs
            RENAME ${json_target_file}
            COMPONENT plugins
        )
    endif()

    # Install plugin dynamic library files
    if(WIN32)
        install(TARGETS ${plugin_name}
            RUNTIME DESTINATION bin/plugins
            COMPONENT plugins
        )
    else()
        install(TARGETS ${plugin_name}
            LIBRARY DESTINATION bin/plugins
            COMPONENT plugins
        )
    endif()
endfunction()

# List all the directories and add subdirectories to the project
function(add_subdirectories_to_project)
    # Get all entries (files and directories) in the current directory
    file(GLOB all_entries RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

    foreach(entry ${all_entries})
        # Concatenate full path
        set(full_path "${CMAKE_CURRENT_SOURCE_DIR}/${entry}")

        # Process only existing directories, excluding files
        if(IS_DIRECTORY "${full_path}" AND NOT entry MATCHES "^\\.")
            # Exclude build directory and CMakeLists.txt (if misjudged)
            if(NOT entry STREQUAL "build" AND NOT entry STREQUAL "CMakeLists.txt" AND NOT entry STREQUAL "repo")
                # Check if the directory contains CMakeLists.txt before adding
                if(EXISTS "${full_path}/CMakeLists.txt")
                    add_subdirectory(${entry})
                    message(STATUS "Added subdirectory: ${entry}")
                else()
                    message(STATUS "Skipped directory without CMakeLists.txt: ${entry}")
                endif()
            else()
                message(STATUS "Skipped directory: ${entry}")
            endif()
        else()
            # Print skipped files (for debugging)
            message(STATUS "Skipped non-directory: ${entry}")
        endif()
    endforeach()
endfunction()

add_subdirectories_to_project()