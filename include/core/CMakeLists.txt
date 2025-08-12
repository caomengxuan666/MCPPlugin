add_library(mcp_core executable_path.cpp)

# 设置POSITION_INDEPENDENT_CODE属性，以支持在共享库中使用
set_property(TARGET mcp_core PROPERTY POSITION_INDEPENDENT_CODE ON)