# src/protocol/CMakeLists.txt
set(PROTOCOL_SOURCES
    json_rpc.cpp
)

add_library(mcp_protocol ${PROTOCOL_SOURCES})
