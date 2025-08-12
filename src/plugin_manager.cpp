#include "plugin_manager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>


namespace fs = std::filesystem;
using json = nlohmann::json;


void PluginManager::setSelfRepoURL(const std::string &url) {
    std::lock_guard<std::mutex> lock(mutex_);
    self_repo_url_ = url;
}

std::string PluginManager::getSelfRepoURL() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return self_repo_url_;
}

bool PluginManager::updateSelf() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (self_repo_url_.empty()) {
        std::cerr << "Self repository URL not set" << std::endl;
        return false;
    }

    auto [owner, repo] = parseGitHubURL(self_repo_url_);
    if (owner.empty() || repo.empty()) {
        std::cerr << "Invalid self repository URL: " << self_repo_url_ << std::endl;
        return false;
    }

    std::cout << "Updating self from repository: " << owner << "/" << repo << std::endl;

    // For now, we just print information about the latest release
    // In a real implementation, you might download and replace the current executable
    httplib::Client cli("https://api.github.com");
    auto res = cli.Get("/repos/" + owner + "/" + repo + "/releases/latest");

    if (!res || res->status != 200) {
        std::cerr << "Failed to get latest release info for " << owner << "/" << repo << std::endl;
        return false;
    }

    try {
        auto json_response = json::parse(res->body);
        std::string tag_name = json_response.value("tag_name", "unknown");
        std::string name = json_response.value("name", "unknown");
        std::string published_at = json_response.value("published_at", "unknown");

        std::cout << "Latest release info:" << std::endl;
        std::cout << "  Tag: " << tag_name << std::endl;
        std::cout << "  Name: " << name << std::endl;
        std::cout << "  Published at: " << published_at << std::endl;

        if (json_response.contains("assets") && json_response["assets"].is_array()) {
            std::cout << "  Assets:" << std::endl;
            for (const auto &asset: json_response["assets"]) {
                std::string asset_name = asset.value("name", "unknown");
                std::string asset_url = asset.value("browser_download_url", "unknown");
                std::cout << "    - " << asset_name << ": " << asset_url << std::endl;
            }
        }

        return true;
    } catch (const std::exception &e) {
        std::cerr << "Error parsing GitHub API response: " << e.what() << std::endl;
        return false;
    }
}

bool PluginManager::addPluginFromGitHub(const std::string &github_url) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [owner, repo] = parseGitHubURL(github_url);
    if (owner.empty() || repo.empty()) {
        std::cerr << "Invalid GitHub URL: " << github_url << std::endl;
        return false;
    }

    // Create plugins directory if not exists
    fs::create_directories("plugins");

    // Generate plugin ID and file path
    std::string plugin_id = owner + "_" + repo;
    std::string plugin_file = "plugins/" + plugin_id + ".plugin";

    // Check if plugin already exists
    for (const auto &plugin: plugins_) {
        if (plugin.id == plugin_id) {
            std::cout << "Plugin " << plugin_id << " already exists" << std::endl;
            return false;
        }
    }

    // Download release
    if (!downloadRelease(owner, repo, plugin_file)) {
        std::cerr << "Failed to download release from " << github_url << std::endl;
        return false;
    }

    // Parse plugin manifest
    PluginInfo plugin_info;
    plugin_info.id = plugin_id;
    plugin_info.url = github_url;
    plugin_info.file_path = plugin_file;

    if (!parsePluginManifest(plugin_file, plugin_info)) {
        std::cerr << "Failed to parse plugin manifest for " << plugin_id << std::endl;
        fs::remove(plugin_file);
        return false;
    }

    plugins_.push_back(plugin_info);
    savePlugins();

    std::cout << "Successfully added plugin: " << plugin_id << std::endl;
    return true;
}

const std::vector<PluginInfo> &PluginManager::getPlugins() const {
    return plugins_;
}

const PluginInfo *PluginManager::getPluginById(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &plugin: plugins_) {
        if (plugin.id == id) {
            return &plugin;
        }
    }
    return nullptr;
}

bool PluginManager::removePlugin(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
        if (it->id == id) {
            // Remove plugin file
            std::error_code ec;
            fs::remove(it->file_path, ec);
            if (ec) {
                std::cerr << "Warning: Failed to remove plugin file: " << it->file_path << std::endl;
            }

            plugins_.erase(it);
            savePlugins();
            return true;
        }
    }
    return false;
}

void PluginManager::updatePlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Updating " << plugins_.size() << " plugins..." << std::endl;

    for (auto &plugin: plugins_) {
        auto [owner, repo] = parseGitHubURL(plugin.url);
        if (!owner.empty() && !repo.empty()) {
            std::cout << "Updating plugin: " << plugin.id << std::endl;
            if (downloadRelease(owner, repo, plugin.file_path)) {
                // Re-parse manifest in case it changed
                parsePluginManifest(plugin.file_path, plugin);
            }
        }
    }

    savePlugins();
    std::cout << "Plugin update completed." << std::endl;
}

std::pair<std::string, std::string> PluginManager::parseGitHubURL(const std::string &url) const {
    // Simple GitHub URL parser
    // Expected format: https://github.com/owner/repo or https://github.com/owner/repo.git
    std::string owner, repo;

    size_t github_pos = url.find("github.com");
    if (github_pos == std::string::npos) {
        return {owner, repo};
    }

    size_t start = github_pos + 11;// "github.com/" length
    size_t first_slash = url.find('/', start);
    if (first_slash == std::string::npos) {
        return {owner, repo};
    }

    owner = url.substr(start, first_slash - start);

    size_t second_slash = url.find('/', first_slash + 1);
    if (second_slash == std::string::npos) {
        repo = url.substr(first_slash + 1);
    } else {
        repo = url.substr(first_slash + 1, second_slash - first_slash - 1);
    }

    // Remove .git suffix if present
    if (repo.length() > 4 && repo.substr(repo.length() - 4) == ".git") {
        repo = repo.substr(0, repo.length() - 4);
    }

    return {owner, repo};
}

bool PluginManager::downloadRelease(const std::string &owner, const std::string &repo, const std::string &save_path) {
    httplib::Client cli("https://api.github.com");
    auto res = cli.Get("/repos/" + owner + "/" + repo + "/releases/latest");

    if (!res || res->status != 200) {
        std::cerr << "Failed to get latest release info for " << owner << "/" << repo << std::endl;
        return false;
    }

    try {
        auto json_response = json::parse(res->body);
        if (!json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty()) {
            std::cerr << "No assets found in the latest release for " << owner << "/" << repo << std::endl;
            return false;
        }

        // Get the first asset download URL
        std::string download_url = json_response["assets"][0]["browser_download_url"];
        std::string asset_name = json_response["assets"][0]["name"];

        std::cout << "Downloading " << asset_name << " from " << download_url << std::endl;

        // Download the asset
        httplib::Client download_cli(download_url);
        auto download_res = download_cli.Get("");

        if (!download_res || download_res->status != 200) {
            std::cerr << "Failed to download asset from " << download_url << std::endl;
            return false;
        }

        // Save to file
        std::ofstream file(save_path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to create file: " << save_path << std::endl;
            return false;
        }

        file.write(download_res->body.c_str(), download_res->body.length());
        file.close();

        std::cout << "Successfully downloaded to " << save_path << std::endl;
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Error parsing GitHub API response: " << e.what() << std::endl;
        return false;
    }
}

bool PluginManager::parsePluginManifest(const std::string &plugin_path, PluginInfo &plugin_info) {
    // In a real implementation, this would parse the plugin file and extract manifest information
    // For now, we'll simulate with some default data

    // Extract repo name from path for demo purposes
    size_t last_slash = plugin_path.rfind('/');
    size_t last_dot = plugin_path.rfind('.');
    std::string repo_name = plugin_path.substr(last_slash + 1, last_dot - last_slash - 1);

    plugin_info.name = repo_name + " Plugin";
    plugin_info.version = "1.0.0";
    plugin_info.description = "A plugin downloaded from GitHub repository " + repo_name;
    plugin_info.release_date = "2025-08-12";

    // Add some sample tools
    ToolInfo tool1{
            "get_info",
            "Get information about this plugin",
            R"({"type": "object", "properties": {}, "required": []})",
            false};

    ToolInfo tool2{
            "process_data",
            "Process data with this plugin",
            R"({"type": "object", "properties": {"data": {"type": "string"}}, "required": ["data"]})",
            false};

    plugin_info.tools.push_back(tool1);
    plugin_info.tools.push_back(tool2);

    return true;
}

void PluginManager::savePlugins() const {
    json j;
    j["plugins"] = json::array();

    for (const auto &plugin: plugins_) {
        json plugin_json;
        plugin_json["id"] = plugin.id;
        plugin_json["name"] = plugin.name;
        plugin_json["version"] = plugin.version;
        plugin_json["description"] = plugin.description;
        plugin_json["url"] = plugin.url;
        plugin_json["file_path"] = plugin.file_path;
        plugin_json["release_date"] = plugin.release_date;
        plugin_json["enabled"] = plugin.enabled;

        plugin_json["tools"] = json::array();
        for (const auto &tool: plugin.tools) {
            json tool_json;
            tool_json["name"] = tool.name;
            tool_json["description"] = tool.description;
            tool_json["parameters"] = tool.parameters;
            tool_json["is_streaming"] = tool.is_streaming;
            plugin_json["tools"].push_back(tool_json);
        }

        j["plugins"].push_back(plugin_json);
    }

    std::ofstream file(plugins_file_);
    if (file) {
        file << j.dump(2);
        file.close();
    }
}

void PluginManager::loadPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(plugins_file_);
    if (!file) {
        return;
    }

    try {
        json j;
        file >> j;
        file.close();

        plugins_.clear();

        if (j.contains("plugins") && j["plugins"].is_array()) {
            for (const auto &plugin_json: j["plugins"]) {
                PluginInfo plugin;
                plugin.id = plugin_json.value("id", "");
                plugin.name = plugin_json.value("name", "");
                plugin.version = plugin_json.value("version", "");
                plugin.description = plugin_json.value("description", "");
                plugin.url = plugin_json.value("url", "");
                plugin.file_path = plugin_json.value("file_path", "");
                plugin.release_date = plugin_json.value("release_date", "");
                plugin.enabled = plugin_json.value("enabled", true);

                if (plugin_json.contains("tools") && plugin_json["tools"].is_array()) {
                    for (const auto &tool_json: plugin_json["tools"]) {
                        ToolInfo tool;

                        // 存储字符串以便 ToolInfo 中的 const char* 字段可以引用
                        plugin.tool_names.push_back(tool_json.value("name", std::string("")));
                        plugin.tool_descriptions.push_back(tool_json.value("description", std::string("")));
                        plugin.tool_parameters.push_back(tool_json.value("parameters", std::string("")));

                        // 现在可以安全地使用 c_str()，因为字符串存储在 plugin 对象中
                        tool.name = plugin.tool_names.back().c_str();
                        tool.description = plugin.tool_descriptions.back().c_str();
                        tool.parameters = plugin.tool_parameters.back().c_str();

                        tool.is_streaming = tool_json.value("is_streaming", false);
                        plugin.tools.push_back(tool);
                    }
                }

                plugins_.push_back(plugin);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error loading plugins: " << e.what() << std::endl;
    }
}

void PluginManager::startServer(int port) {
    loadPlugins();
    // CORS headers
    srv_.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (req.method == "OPTIONS") {
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "*");
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Endpoint to list all plugins
    srv_.Get("/plugins", [this](const httplib::Request &req, httplib::Response &res) {
        json response;
        response["plugins"] = json::array();

        for (const auto &plugin: plugins_) {
            json plugin_json;
            plugin_json["id"] = plugin.id;
            plugin_json["name"] = plugin.name;
            plugin_json["version"] = plugin.version;
            plugin_json["description"] = plugin.description;
            plugin_json["release_date"] = plugin.release_date;
            plugin_json["enabled"] = plugin.enabled;

            plugin_json["tools"] = json::array();
            for (const auto &tool: plugin.tools) {
                json tool_json;
                tool_json["name"] = tool.name;
                tool_json["description"] = tool.description;
                tool_json["parameters"] = json::parse(tool.parameters);
                tool_json["is_streaming"] = tool.is_streaming;
                plugin_json["tools"].push_back(tool_json);
            }

            response["plugins"].push_back(plugin_json);
        }

        res.set_content(response.dump(), "application/json");
    });

    // Endpoint to get a specific plugin
    srv_.Get(R"(/plugins/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string plugin_id = req.matches[1];
        const PluginInfo *plugin = getPluginById(plugin_id);

        if (!plugin) {
            res.status = 404;
            json error{{"error", "Plugin not found"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        json plugin_json;
        plugin_json["id"] = plugin->id;
        plugin_json["name"] = plugin->name;
        plugin_json["version"] = plugin->version;
        plugin_json["description"] = plugin->description;
        plugin_json["release_date"] = plugin->release_date;
        plugin_json["enabled"] = plugin->enabled;

        plugin_json["tools"] = json::array();
        for (const auto &tool: plugin->tools) {
            json tool_json;
            tool_json["name"] = tool.name;
            tool_json["description"] = tool.description;
            tool_json["parameters"] = json::parse(tool.parameters);
            tool_json["is_streaming"] = tool.is_streaming;
            plugin_json["tools"].push_back(tool_json);
        }

        res.set_content(plugin_json.dump(), "application/json");
    });

    // Endpoint to download a plugin
    srv_.Get(R"(/plugins/([^/]+)/download)", [this](const httplib::Request &req, httplib::Response &res) {
        std::string plugin_id = req.matches[1];
        const PluginInfo *plugin = getPluginById(plugin_id);

        if (!plugin) {
            res.status = 404;
            json error{{"error", "Plugin not found"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        std::ifstream file(plugin->file_path, std::ios::binary);
        if (!file) {
            res.status = 500;
            json error{{"error", "Cannot read plugin file"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        std::string buffer((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        res.set_content(buffer, "application/octet-stream");
        res.set_header("Content-Disposition", "attachment; filename=\"" + plugin_id + ".plugin\"");
    });

    // Endpoint to add a plugin from GitHub
    srv_.Post("/plugins", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            json request_body = json::parse(req.body);
            std::string github_url = request_body.value("github_url", "");

            if (github_url.empty()) {
                res.status = 400;
                json error{{"error", "Missing github_url parameter"}};
                res.set_content(error.dump(), "application/json");
                return;
            }

            if (addPluginFromGitHub(github_url)) {
                res.status = 201;
                json response{{"message", "Plugin added successfully"}};
                res.set_content(response.dump(), "application/json");
            } else {
                res.status = 500;
                json error{{"error", "Failed to add plugin"}};
                res.set_content(error.dump(), "application/json");
            }
        } catch (const std::exception &e) {
            res.status = 400;
            json error{{"error", "Invalid JSON in request body"}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // Endpoint to remove a plugin
    srv_.Delete(R"(/plugins/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string plugin_id = req.matches[1];

        if (removePlugin(plugin_id)) {
            res.status = 200;
            json response{{"message", "Plugin removed successfully"}};
            res.set_content(response.dump(), "application/json");
        } else {
            res.status = 404;
            json error{{"error", "Plugin not found"}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // Endpoint to update all plugins
    srv_.Post("/plugins/update", [this](const httplib::Request &req, httplib::Response &res) {
        updatePlugins();
        res.status = 200;
        json response{{"message", "Plugins updated successfully"}};
        res.set_content(response.dump(), "application/json");
    });

    // Endpoint to get self repository info
    srv_.Get("/self", [this](const httplib::Request &req, httplib::Response &res) {
        json response;
        response["self_repo_url"] = getSelfRepoURL();
        res.set_content(response.dump(), "application/json");
    });
    srv_.Get("/self/latest/info", [this](const httplib::Request &req, httplib::Response &res) {
        auto latest = getLatestReleaseInfo();
        if (!latest.has_value()) {
            res.status = 404;
            res.set_content("{\"error\": \"No latest release found\"}", "application/json");
            return;
        }

        json response;
        response["tag_name"] = latest->tag_name;
        response["name"] = latest->name;
        response["published_at"] = latest->published_at;
        response["assets"] = json::array();

        for (const auto &asset: latest->assets) {
            json asset_json;
            asset_json["name"] = asset.name;
            asset_json["download_url"] = asset.download_url;
            asset_json["platform"] = (asset.platform == Platform::Windows) ? "windows" : (asset.platform == Platform::Linux) ? "linux"
                                                                                                                             : "unknown";
            response["assets"].push_back(asset_json);
        }

        res.set_content(response.dump(2), "application/json");
    });

    srv_.Get(R"(/self/latest/download/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string platform_str = req.matches[1];
        Platform target_platform = Platform::Unknown;

        if (platform_str == "windows") {
            target_platform = Platform::Windows;
        } else if (platform_str == "linux") {
            target_platform = Platform::Linux;
        }

        if (target_platform == Platform::Unknown) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid platform (use 'windows' or 'linux')\"}", "application/json");
            return;
        }

        auto latest = getLatestReleaseInfo();
        if (!latest.has_value()) {
            res.status = 404;
            res.set_content("{\"error\": \"No latest release found\"}", "application/json");
            return;
        }

        std::string platform_dir;
        switch (target_platform) {
            case Platform::Windows:
                platform_dir = "windows/";
                break;
            case Platform::Linux:
                platform_dir = "linux/";
                break;
            default:
                res.status = 500;
                res.set_content("{\"error\": \"Internal platform error\"}", "application/json");
                return;
        }

        for (const auto &asset: latest->assets) {
            if (asset.platform != target_platform) {
                continue;
            }

            std::string local_path = update_dir_ + "/" + platform_dir + asset.name;
            std::replace(local_path.begin(), local_path.end(), '\\', '/');
            size_t pos;
            while ((pos = local_path.find("//")) != std::string::npos) {
                local_path.replace(pos, 2, "/");
            }

            if (fs::exists(local_path)) {
                std::ifstream file(local_path, std::ios::binary);
                if (file.good()) {
                    std::string buffer((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    res.set_content(buffer, "application/octet-stream");
                    res.set_header("Content-Disposition",
                                   "attachment; filename=\"" + asset.name + "\"");
                    return;
                } else {
                    res.status = 500;
                    res.set_content("{\"error\": \"Failed to read asset file\"}", "application/json");
                    return;
                }
            }
        }

        res.status = 404;
        res.set_content("{\"error\": \"No asset found for platform\"}", "application/json");
    });
    // Endpoint to update self from repository
    srv_.Post("/self/update", [this](const httplib::Request &req, httplib::Response &res) {
        if (updateSelf()) {
            res.status = 200;
            json response{{"message", "Self update check completed"}};
            res.set_content(response.dump(), "application/json");
        } else {
            res.status = 500;
            json error{{"error", "Failed to check for self updates"}};
            res.set_content(error.dump(), "application/json");
        }
    });

    std::cout << "Server starting on port " << port << "..." << std::endl;
    srv_.listen("0.0.0.0", port);
}

PluginManager::PluginManager() {
}

PluginManager::~PluginManager() {
    srv_.stop();
}

void PluginManager::stopServer() {
    srv_.stop();
}
std::optional<ReleaseInfo> PluginManager::getLatestReleaseInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);


    if (release_history_.empty()) {
        return std::nullopt;
    }

    auto latest_it = std::max_element(
            release_history_.begin(),
            release_history_.end(),
            [](const auto &a, const auto &b) {
                return a.first < b.first;
            });

    if (latest_it != release_history_.end()) {
        return latest_it->second;
    } else {
        return std::nullopt;
    }
}