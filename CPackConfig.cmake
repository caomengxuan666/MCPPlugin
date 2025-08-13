# Basic information
set(CPACK_PACKAGE_NAME "MCPPlugin")
set(CPACK_PACKAGE_VENDOR "MCP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MCP Plugin System")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_CONTACT "example@example.com")

# Use ZIP generator
set(CPACK_GENERATOR "ZIP")

# Enable component installation (required)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON) # Enable component installation for archive generators
set(CPACK_COMPONENTS_ALL plugins server)
set(CPACK_MONOLITHIC_INSTALL OFF)

# Component groups (optional)
set(CPACK_COMPONENT_GROUPS_ALL "Runtime")
set(CPACK_COMPONENT_GROUP_RUNTIME_DESCRIPTION "MCP Runtime Components")
set(CPACK_COMPONENT_GROUP_RUNTIME_EXPANDED ON)

# Plugin component configuration
set(CPACK_COMPONENT_PLUGINS_DISPLAY_NAME "MCP Plugins")
set(CPACK_COMPONENT_PLUGINS_DESCRIPTION "MCP Plugins with dynamic libraries and configuration files")
set(CPACK_COMPONENT_PLUGINS_GROUP "Runtime")
set(CPACK_COMPONENT_PLUGINS_FILE_NAME "MCPPlugin-plugins-1.0.0")

# Server component configuration
set(CPACK_COMPONENT_SERVER_DISPLAY_NAME "MCP Server")
set(CPACK_COMPONENT_SERVER_DESCRIPTION "MCP Server and plugin control tool")
set(CPACK_COMPONENT_SERVER_GROUP "Runtime")
set(CPACK_COMPONENT_SERVER_FILE_NAME "MCPPlugin-server-1.0.0")

# Disable extra collection packages
set(CPACK_COMPONENT_ENABLE_SETS OFF)