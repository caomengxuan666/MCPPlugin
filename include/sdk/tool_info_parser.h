// plugins/sdk/tool_info_parser.h

#ifndef B2C65C24_24D8_4AF0_AA25_FCAC02ECF845
#define B2C65C24_24D8_4AF0_AA25_FCAC02ECF845

#include "mcp_plugin.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class ToolInfoParser {
public:
    static std::vector<ToolInfo> loadFromFile(const std::string &json_file_path);

    static std::vector<ToolInfo> parseFromString(const std::string &json_string);

    static std::vector<ToolInfo> parseFromJson(const nlohmann::json &json_data);

    static void freeToolInfoVector(std::vector<ToolInfo> &tools);

private:
    static std::vector<ToolInfo> parseTools(const nlohmann::json &tools_json);
    static ToolInfo parseTool(const nlohmann::json &tool_json);
    static char *stringToCharPtr(const std::string &str);
};


#endif /* B2C65C24_24D8_4AF0_AA25_FCAC02ECF845 */
