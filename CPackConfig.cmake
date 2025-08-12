# CPack配置文件，用于将软件包分为两个组件

# 设置包的基本信息
set(CPACK_PACKAGE_NAME "MCPPlugin")
set(CPACK_PACKAGE_VENDOR "MCP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MCP Plugin System")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_CONTACT "example@example.com")

# 设置包的生成类型
set(CPACK_GENERATOR "ZIP")

# 启用组件安装
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# 定义组件
# 1. 插件组件 - 只包含插件动态库和对应的JSON文件
set(CPACK_COMPONENT_PLUGINS_DISPLAY_NAME "MCP Plugins")
set(CPACK_COMPONENT_PLUGINS_DESCRIPTION "MCP Plugins with dynamic libraries and configuration files")
set(CPACK_COMPONENT_PLUGINS_GROUP "Runtime")

# 2. 服务器组件 - 包含服务器和plugin_ctl工具
set(CPACK_COMPONENT_SERVER_DISPLAY_NAME "MCP Server")
set(CPACK_COMPONENT_SERVER_DESCRIPTION "MCP Server and plugin control tool")
set(CPACK_COMPONENT_SERVER_GROUP "Runtime")

# 设置组件之间的依赖关系
# 插件组件不依赖其他组件
# 服务器组件也不依赖其他组件，可以独立安装

# 组件分组
set(CPACK_COMPONENT_GROUPS_ALL "Runtime")
set(CPACK_COMPONENT_GROUP_RUNTIME_DESCRIPTION "MCP Runtime Components")
set(CPACK_COMPONENT_GROUP_RUNTIME_EXPANDED ON)