// Plugin: example_plugin
// This is a template for your plugin implementation.
#include "core/mcpserver_api.h"
#include "mcp_plugin.h"
#include "tool_info_parser.h"
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>

static std::vector<ToolInfo> g_tools;

// Generator structure for streaming tools
struct example_pluginGenerator {
    // Add your generator fields here
    bool running = true;
    std::string error;
};

// Generator next function for streaming tools
static int example_plugin_next(void* generator, const char** result_json, MCPError *error) {
    // Add your streaming logic here
    // Return 0 to continue streaming, 1 to stop
    if (!generator) {
        *result_json = R"({"error": "Invalid generator pointer"})";
        if (error) {
            error->code = 1;
            error->message = "Invalid generator pointer";
        }
        return 1;
    }

    auto* gen = static_cast<example_pluginGenerator*>(generator);
    
    // Check if there's an error
    if (!gen->error.empty()) {
        *result_json = gen->error.c_str();
        if (error) {
            error->code = 2;
            error->message = gen->error.c_str();
        }
        return 1;
    }

    // Check if streaming should stop
    if (!gen->running) {
        *result_json = nullptr;
        return 1;
    }

    // TODO: Implement your streaming logic here
    // Example streaming response:
    static thread_local std::string buffer;
    buffer = nlohmann::json({{"jsonrpc", "2.0"},
                             {"method", "text"},
                             {"params", {{"text", "Example streamed content"}}}})
                     .dump();

    *result_json = buffer.c_str();
    if (error) {
        error->code = 0; // No error
        error->message = nullptr;
    }
    return 0; // Continue streaming
}

// Generator free function for streaming tools
static void example_plugin_free(void* generator) {
    auto* gen = static_cast<example_pluginGenerator*>(generator);
    if (gen) {
        gen->running = false;
        delete gen;
    }
}

extern "C" MCP_API ToolInfo *get_tools(int *count) {
    try {
        // Load tool information if not already loaded
        if (g_tools.empty()) {
            g_tools = ToolInfoParser::loadFromFile("tools.json");
        }

        *count = static_cast<int>(g_tools.size());
        return g_tools.data();
    } catch (const std::exception &e) {
        *count = 0;
        return nullptr;
    }
}

extern "C" MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        // TODO: Implement your tool logic here
        if (tool_name == "example_plugin") {
            // Example implementation - replace with your actual logic
            std::string result = nlohmann::json{{"result", "Hello from example_plugin"}}.dump();
            return strdup(result.c_str());
        }

        // For streaming tools, return the generator
        // Example for a streaming tool:
        // if (tool_name == "stream_example_plugin") {
        //     auto* gen = new example_pluginGenerator();
        //     // Initialize your generator here
        //     return reinterpret_cast<const char*>(gen);
        // }

        if (error) {
            error->code = 3;
            error->message = ("Unknown tool: " + tool_name).c_str();
        }
        return strdup((nlohmann::json{{"error", "Unknown tool: " + tool_name}}.dump()).c_str());
    } catch (const std::exception &e) {
        if (error) {
            error->code = 4;
            error->message = e.what();
        }
        return strdup((nlohmann::json{{"error", e.what()}}.dump()).c_str());
    }
}

extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}

// For streaming tools, implement these functions
extern "C" MCP_API StreamGeneratorNext get_stream_next() {
    return reinterpret_cast<StreamGeneratorNext>(example_plugin_next);
}

extern "C" MCP_API StreamGeneratorFree get_stream_free() {
    return reinterpret_cast<StreamGeneratorFree>(example_plugin_free);
}
