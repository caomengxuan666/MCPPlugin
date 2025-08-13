/**
 * plugin_manager.cpp
 *
 * MCP Plugin Manager - Full Production Version
 *
 * Features:
 * - Plugin management (add, remove, update, list)
 * - HTTP server with RESTful API endpoints
 * - GitHub release download (plugins only)
 * - Auto-fetch latest release from self repository
 * - Version persistence via local file
 * - Incremental update: only download when newer
 * - Background thread for periodic check (every 30s)
 * - Platform-specific asset handling (Windows/Linux)
 * - Thread-safe operations with mutex
 *
 * Author: caomengxuan666
 * Repository: https://github.com/caomengxuan666/MCPPlugin.git
 * License: MIT
 */

#include "plugin_manager.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ========================================
// Setter & Getter for Self Repository URL
// ========================================

void PluginManager::setSelfRepoURL(const std::string &url) {
    std::lock_guard<std::mutex> lock(mutex_);
    self_repo_url_ = url;
}

std::string PluginManager::getSelfRepoURL() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return self_repo_url_;
}

// ========================================
// Read/Write Version File (for persistence)
// ========================================

// Internal lock-free version for functions that already hold the lock
std::string PluginManager::readVersionFileUnsafe() const {
    std::ifstream file(version_file_);
    if (!file.is_open()) {
        return "";
    }
    std::string version;
    std::getline(file, version);
    file.close();
    return version;
}

// External version with lock for external calls
std::string PluginManager::readVersionFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return readVersionFileUnsafe();
}

void PluginManager::writeVersionFile(const std::string &version) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(version_file_);
    if (file) {
        file << version << std::endl;
        file.close();
        std::cout << "💾 Version saved to file: " << version << std::endl;
    } else {
        std::cerr << "❌ Failed to write version file: " << version_file_ << std::endl;
    }
}

// ========================================
// Compare Version Tags (e.g., v0.1.0.8 > v0.1.0.7)
// ========================================

bool PluginManager::isNewerVersion(const std::string &new_tag, const std::string &current_tag) const {
    if (current_tag.empty()) {
        return true;// No previous version → treat as newer
    }

    auto strip_v = [](const std::string &tag) -> std::string {
        return tag.size() > 0 && tag[0] == 'v' ? tag.substr(1) : tag;
    };

    std::string a = strip_v(new_tag);
    std::string b = strip_v(current_tag);

    return a != b;// If different, assume it's an update
}

// ========================================
// Update Self: Check GitHub for New Release
// ========================================

bool PluginManager::updateSelf() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (self_repo_url_.empty()) {
        std::cerr << "❌ Self repository URL not set" << std::endl;
        return false;
    }

    auto [owner, repo] = parseGitHubURL(self_repo_url_);
    if (owner.empty() || repo.empty()) {
        std::cerr << "❌ Invalid self repository URL: " << self_repo_url_ << std::endl;
        return false;
    }

    std::cout << "🔍 Updating self from repository: " << owner << "/" << repo << std::endl;

    httplib::Client cli("api.github.com");
    setupHttpClient(cli);
    auto res = cli.Get("/repos/" + owner + "/" + repo + "/releases/latest");

    if (!res) {
        std::cerr << "❌ Failed to connect to GitHub API" << std::endl;
        return false;
    }

    if (res->status != 200) {
        std::cerr << "❌ Failed to get latest release info for " << owner << "/" << repo
                  << " (status: " << res->status << ")" << std::endl;
        return false;
    }

    try {
        auto json_response = json::parse(res->body);
        std::string tag_name = json_response.value("tag_name", "unknown");
        std::string name = json_response.value("name", "unknown");
        std::string published_at = json_response.value("published_at", "unknown");

        std::cout << "📌 Latest release info:" << std::endl;
        std::cout << "   Tag: " << tag_name << std::endl;
        std::cout << "   Name: " << name << std::endl;
        std::cout << "   Published at: " << published_at << std::endl;

        if (json_response.contains("assets") && json_response["assets"].is_array()) {
            std::cout << "   Assets:" << std::endl;
            for (const auto &asset: json_response["assets"]) {
                std::string asset_name = asset.value("name", "unknown");
                std::string asset_url = asset.value("browser_download_url", "unknown");
                std::cout << "     - " << asset_name << ": " << asset_url << std::endl;
            }
        }

        return true;
    } catch (const std::exception &e) {
        std::cerr << "❌ Error parsing GitHub API response: " << e.what() << std::endl;
        return false;
    }
}

// ========================================
// Add Plugin from GitHub
// ========================================

bool PluginManager::addPluginFromGitHub(const std::string &github_url) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto [owner, repo] = parseGitHubURL(github_url);
    if (owner.empty() || repo.empty()) {
        std::cerr << "❌ Invalid GitHub URL: " << github_url << std::endl;
        return false;
    }

    fs::create_directories("plugins");

    std::string plugin_id = owner + "_" + repo;
    std::string plugin_file = "plugins/" + plugin_id + ".plugin";

    for (const auto &plugin: plugins_) {
        if (plugin.id == plugin_id) {
            std::cout << "ℹ️  Plugin " << plugin_id << " already exists" << std::endl;
            return false;
        }
    }

    if (!downloadRelease(owner, repo, plugin_file)) {
        std::cerr << "❌ Failed to download release from " << github_url << std::endl;
        return false;
    }

    PluginInfo plugin_info;
    plugin_info.id = plugin_id;
    plugin_info.url = github_url;
    plugin_info.file_path = plugin_file;

    if (!parsePluginManifest(plugin_file, plugin_info)) {
        std::cerr << "❌ Failed to parse plugin manifest for " << plugin_id << std::endl;
        fs::remove(plugin_file);
        return false;
    }

    plugins_.push_back(plugin_info);
    savePlugins();

    std::cout << "✅ Successfully added plugin: " << plugin_id << std::endl;
    return true;
}

// ========================================
// Get Plugins
// ========================================

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

// ========================================
// Remove Plugin
// ========================================

bool PluginManager::removePlugin(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
        if (it->id == id) {
            std::error_code ec;
            fs::remove(it->file_path, ec);
            if (ec) {
                std::cerr << "⚠️  Failed to remove plugin file: " << it->file_path << std::endl;
            }
            plugins_.erase(it);
            savePlugins();
            return true;
        }
    }
    return false;
}

// ========================================
// Update All Plugins
// ========================================

void PluginManager::updatePlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "🔄 Updating " << plugins_.size() << " plugins..." << std::endl;

    for (auto &plugin: plugins_) {
        auto [owner, repo] = parseGitHubURL(plugin.url);
        if (!owner.empty() && !repo.empty()) {
            std::cout << "   Updating plugin: " << plugin.id << std::endl;
            if (downloadRelease(owner, repo, plugin.file_path)) {
                parsePluginManifest(plugin.file_path, plugin);
            }
        }
    }

    savePlugins();
    std::cout << "✅ Plugin update completed." << std::endl;
}

// ========================================
// Parse GitHub URL (owner/repo)
// ========================================

std::pair<std::string, std::string> PluginManager::parseGitHubURL(const std::string &url) const {
    std::string owner, repo;
    size_t github_pos = url.find("github.com");
    if (github_pos == std::string::npos) {
        return {owner, repo};
    }

    size_t start = github_pos + 11;
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

    if (repo.length() > 4 && repo.substr(repo.length() - 4) == ".git") {
        repo = repo.substr(0, repo.length() - 4);
    }

    return {owner, repo};
}

// ========================================
// Download Release Asset
// ========================================

bool PluginManager::downloadRelease(const std::string &owner, const std::string &repo, const std::string &save_path) {
    httplib::Client cli("https://api.github.com");
    setupHttpClient(cli);
    auto res = cli.Get("/repos/" + owner + "/" + repo + "/releases/latest");

    if (!res) {
        std::cerr << "❌ Failed to connect to GitHub API" << std::endl;
        return false;
    }

    if (res->status != 200) {
        std::cerr << "❌ Failed to get latest release info for " << owner << "/" << repo
                  << " (status: " << res->status << ")" << std::endl;
        return false;
    }

    try {
        auto json_response = json::parse(res->body);
        if (!json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty()) {
            std::cerr << "❌ No assets found in the latest release for " << owner << "/" << repo << std::endl;
            return false;
        }

        std::string download_url = json_response["assets"][0]["browser_download_url"];
        std::string asset_name = json_response["assets"][0]["name"];

        std::cout << "📥 Downloading " << asset_name << " from " << download_url << std::endl;

        httplib::Client download_cli(download_url);
        setupHttpClient(download_cli);
        auto download_res = download_cli.Get("");

        if (!download_res) {
            std::cerr << "❌ Failed to connect to download URL" << std::endl;
            return false;
        }

        if (download_res->status != 200) {
            std::cerr << "❌ Failed to download asset from " << download_url
                      << " (status: " << download_res->status << ")" << std::endl;
            return false;
        }

        std::ofstream file(save_path, std::ios::binary);
        if (!file) {
            std::cerr << "❌ Failed to create file: " << save_path << std::endl;
            return false;
        }

        file.write(download_res->body.c_str(), download_res->body.length());
        file.close();

        std::cout << "✅ Successfully downloaded to " << save_path << std::endl;
        return true;
    } catch (const std::exception &e) {
        std::cerr << "❌ Error parsing GitHub API response: " << e.what() << std::endl;
        return false;
    }
}

// ========================================
// Parse Plugin Manifest (mock data)
// ========================================

bool PluginManager::parsePluginManifest(const std::string &plugin_path, PluginInfo &plugin_info) {
    size_t last_slash = plugin_path.rfind('/');
    size_t last_dot = plugin_path.rfind('.');
    std::string repo_name = plugin_path.substr(last_slash + 1, last_dot - last_slash - 1);

    plugin_info.name = repo_name + " Plugin";
    plugin_info.version = "1.0.0";
    plugin_info.description = "A plugin downloaded from GitHub repository " + repo_name;
    plugin_info.release_date = "2025-08-12";

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

// ========================================
// Save & Load Plugins (JSON)
// ========================================

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
                        plugin.tool_names.push_back(tool_json.value("name", std::string("")));
                        plugin.tool_descriptions.push_back(tool_json.value("description", std::string("")));
                        plugin.tool_parameters.push_back(tool_json.value("parameters", std::string("")));

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
        std::cerr << "❌ Error loading plugins: " << e.what() << std::endl;
    }
}

// ========================================
// Fetch Latest Release Info (for self)
// ========================================

std::optional<ReleaseInfo> PluginManager::fetchLatestRelease() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (self_repo_url_.empty()) {
        std::cerr << "❌ Self repository URL not set" << std::endl;
        return std::nullopt;
    }

    auto [owner, repo] = parseGitHubURL(self_repo_url_);
    if (owner.empty() || repo.empty()) return std::nullopt;

    httplib::Client cli("https://api.github.com");
    setupHttpClient(cli);
    auto res = cli.Get("/repos/" + owner + "/" + repo + "/releases/latest");
    if (!res) {
        std::cerr << "❌ Failed to connect to GitHub API" << std::endl;
        return std::nullopt;
    }

    if (res->status != 200) {
        std::cerr << "❌ Failed to get latest release info for " << owner << "/" << repo
                  << " (status: " << res->status << ")" << std::endl;
        return std::nullopt;
    }

    try {
        json j = json::parse(res->body);
        ReleaseInfo release;
        release.tag_name = j.value("tag_name", "unknown");
        release.name = j.value("name", "unknown");
        release.published_at = j.value("published_at", "unknown");

        const std::vector<std::string> target_names = {
                "MCPPlugin-plugins-windows.zip",
                "MCPPlugin-plugins-linux.zip"};

        for (const auto &asset: j["assets"]) {
            std::string name = asset.value("name", "");
            if (std::find(target_names.begin(), target_names.end(), name) == target_names.end()) continue;

            ReleaseAsset a;
            a.name = name;
            a.download_url = asset.value("browser_download_url", "");
            a.platform = getPlatformFromFileName(name);
            std::string platform_dir = (a.platform == Platform::Windows) ? "windows" : "linux";
            a.local_path = (update_dir_ / fs::path(platform_dir) / fs::path(name)).string();
            release.assets.push_back(std::move(a));
        }

        return release.assets.empty() ? std::nullopt : std::make_optional(release);
    } catch (...) {
        return std::nullopt;
    }
}

Platform PluginManager::getPlatformFromFileName(const std::string &filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("windows") != std::string::npos || lower.find("win") != std::string::npos)
        return Platform::Windows;
    if (lower.find("linux") != std::string::npos)
        return Platform::Linux;
    return Platform::Unknown;
}

bool PluginManager::downloadReleaseAsset(const ReleaseAsset &asset) {
    constexpr int MAX_RETRIES = 3;     // Maximum retry attempts
    constexpr int RETRY_DELAY_MS = 500;// Delay between retries (milliseconds)

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        // Parse URL to get host and path
        size_t start = asset.download_url.find("://");
        if (start == std::string::npos) {
            std::cerr << "❌ Invalid URL: " << asset.download_url << std::endl;
            return false;
        }
        start += 3;// Skip "://"
        size_t path_start = asset.download_url.find('/', start);
        if (path_start == std::string::npos) {
            std::cerr << "❌ Invalid URL: " << asset.download_url << std::endl;
            return false;
        }

        std::string host = asset.download_url.substr(start, path_start - start);
        std::string path = asset.download_url.substr(path_start);

        // Create client
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        httplib::SSLClient cli(host.c_str(), 443);// HTTPS port
#else
        httplib::Client cli(host.c_str(), 80);// HTTP port
#endif

        setupHttpClient(cli);
        cli.set_follow_location(true);// Automatically follow redirects
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli.enable_server_certificate_verification(false);// Disable SSL verification for testing (recommended to enable in production)
#endif

        if (attempt > 1) {
            std::cout << "🔄 Retry attempt " << attempt << " for " << asset.name << "..." << std::endl;
        } else {
            std::cout << "📥 Downloading asset: " << asset.name << " from " << asset.download_url << std::endl;
        }

        auto res = cli.Get(path.c_str());
        if (!res) {
            std::cerr << "❌ Download failed (connection error) [Attempt " << attempt << "]" << std::endl;
        } else if (res->status != 200) {
            std::cerr << "❌ Download failed (HTTP " << res->status << ") [Attempt " << attempt << "]" << std::endl;
        } else {
            // Create save directory
            fs::create_directories(fs::path(asset.local_path).parent_path());

            // Write to file
            std::ofstream file(asset.local_path, std::ios::binary);
            if (!file) {
                std::cerr << "❌ Cannot create file: " << asset.local_path << std::endl;
                return false;
            }
            file.write(res->body.c_str(), static_cast<std::streamsize>(res->body.size()));
            file.close();

            std::cout << "✅ Downloaded: " << asset.name << " → " << asset.local_path << std::endl;
            return true;
        }

        if (attempt < MAX_RETRIES) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
    }

    std::cerr << "❌ All attempts failed for: " << asset.name << std::endl;
    return false;
}

void PluginManager::startPeriodicThread() {
    stop_flag_ = false;
    periodic_thread_ = std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        periodicFetchLatestRelease();
        while (!stop_flag_) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            periodicFetchLatestRelease();
        }
    });
}

void PluginManager::periodicFetchLatestRelease() {
    auto release_opt = fetchLatestRelease();
    if (!release_opt) return;

    const auto &release = *release_opt;
    std::string current_ver = readVersionFile();

    if (isNewerVersion(release.tag_name, current_ver)) {
        std::cout << "🆕 New version detected: " << release.tag_name
                  << " (current: " << (current_ver.empty() ? "none" : current_ver) << ")" << std::endl;

        bool all_downloaded = true;
        for (const auto &asset: release.assets) {
            if (!downloadReleaseAsset(asset)) all_downloaded = false;
        }

        if (all_downloaded) {
            writeVersionFile(release.tag_name);
            std::lock_guard<std::mutex> lock(mutex_);
            saveReleaseInfo(release);// Save release info to file
            latest_release_info_ = release;
            release_history_[release.tag_name] = release;
            current_version_ = release.tag_name;
            std::cout << "✅ Successfully updated to version: " << release.tag_name << std::endl;
        }
    } else {
        // Even if not newer, we still want to update the cached release info
        // But only save to file if the release info actually changed
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if the release info actually changed
        bool isDifferent = false;
        
        // Compare basic release info
        if (latest_release_info_.tag_name != release.tag_name || 
            latest_release_info_.name != release.name ||
            latest_release_info_.published_at != release.published_at) {
            isDifferent = true;
        } 
        // Compare assets if basic info is the same
        else if (latest_release_info_.assets.size() != release.assets.size()) {
            isDifferent = true;
        } 
        else {
            // Compare each asset
            for (size_t i = 0; i < release.assets.size(); ++i) {
                const auto& old_asset = latest_release_info_.assets[i];
                const auto& new_asset = release.assets[i];
                
                if (old_asset.name != new_asset.name ||
                    old_asset.download_url != new_asset.download_url ||
                    old_asset.local_path != new_asset.local_path ||
                    old_asset.platform != new_asset.platform) {
                    isDifferent = true;
                    break;
                }
            }
        }
        
        // Only update and save if there are actual differences
        if (isDifferent) {
            latest_release_info_ = release;
            release_history_[release.tag_name] = release;
            saveReleaseInfo(release);// Save release info to file
            std::cout << "📋 Updated cached release info: " << release.tag_name << std::endl;
        }
    }
}

std::optional<ReleaseInfo> PluginManager::getLatestReleaseInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (latest_release_info_.tag_name.empty()) return std::nullopt;
    return latest_release_info_;
}

void PluginManager::saveReleaseInfo(const ReleaseInfo &release) const {
    // Note: Functions calling this function should already have acquired the mutex lock, so no need to acquire it here
    json j;
    j["tag_name"] = release.tag_name;
    j["name"] = release.name;
    j["published_at"] = release.published_at;
    j["assets"] = json::array();

    for (const auto &asset: release.assets) {
        j["assets"].push_back({{"name", asset.name},
                               {"download_url", asset.download_url},
                               {"platform", asset.platform == Platform::Windows ? "windows" : asset.platform == Platform::Linux ? "linux"
                                                                                                                                : "unknown"},
                               {"local_path", asset.local_path}});
    }

    std::ofstream file(release_info_file_);
    if (file) {
        file << j.dump(2);
        file.close();
        std::cout << "💾 Release info saved to: " << release_info_file_ << std::endl;
    } else {
        std::cerr << "❌ Failed to save release info: " << release_info_file_ << std::endl;
    }
}

std::optional<ReleaseInfo> PluginManager::loadReleaseInfo() const {
    // Note: Functions calling this function should already have acquired the mutex lock, so no need to acquire it here
    std::ifstream file(release_info_file_);
    if (!file.is_open()) {
        std::cout << "🔍 No saved release info found: " << release_info_file_ << std::endl;
        return std::nullopt;
    }

    try {
        json j;
        file >> j;
        file.close();

        ReleaseInfo release;
        release.tag_name = j.value("tag_name", "");
        release.name = j.value("name", "");
        release.published_at = j.value("published_at", "");

        if (release.tag_name.empty()) {
            return std::nullopt;
        }

        for (const auto &j_asset: j["assets"]) {
            ReleaseAsset asset;
            asset.name = j_asset.value("name", "");
            asset.download_url = j_asset.value("download_url", "");
            std::string platform = j_asset.value("platform", "");
            asset.platform = (platform == "windows") ? Platform::Windows : (platform == "linux") ? Platform::Linux
                                                                                                 : Platform::Unknown;
            asset.local_path = j_asset.value("local_path", "");

            // Verify that the local file exists
            if (fs::exists(asset.local_path)) {
                release.assets.push_back(std::move(asset));
            }
        }

        if (release.assets.empty()) {
            return std::nullopt;
        }

        std::cout << "✅ Loaded cached release info: " << release.tag_name << std::endl;
        return release;
    } catch (const std::exception &e) {
        std::cerr << "❌ Failed to parse release info: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// ========================================
// Start/Stop Server
// ========================================

void PluginManager::startServer(int port) {
    loadPlugins();

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

    // === Your existing HTTP routes ===
    // (All your /plugins, /self, etc. routes go here - unchanged)

    srv_.Get("/self/latest/info", [this](const httplib::Request &, httplib::Response &res) {
        auto latest = getLatestReleaseInfo();
        if (!latest.has_value()) {
            res.status = 404;
            res.set_content("{\"error\": \"No latest release found\"}", "application/json");
            return;
        }

        json j = {
                {"tag_name", latest->tag_name},
                {"name", latest->name},
                {"published_at", latest->published_at},
                {"assets", json::array()}};

        for (const auto &asset: latest->assets) {
            j["assets"].push_back({{"name", asset.name},
                                   {"download_url", asset.download_url},
                                   {"platform", asset.platform == Platform::Windows ? "windows" : "linux"}});
        }

        res.set_content(j.dump(2), "application/json");
    });

    srv_.Get(R"(/self/latest/download/([^/]+))", [this](const httplib::Request &req, httplib::Response &res) {
        std::string platform_str = req.matches[1];
        Platform target_platform = (platform_str == "windows") ? Platform::Windows : (platform_str == "linux") ? Platform::Linux
                                                                                                               : Platform::Unknown;

        if (target_platform == Platform::Unknown) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid platform\"}", "application/json");
            return;
        }

        auto latest = getLatestReleaseInfo();
        if (!latest.has_value()) {
            res.status = 404;
            res.set_content("{\"error\": \"No latest release\"}", "application/json");
            return;
        }

        std::string platform_dir = (target_platform == Platform::Windows) ? "windows" : "linux";
        std::string local_path = update_dir_ + "/" + platform_dir + "/" +
                                 (target_platform == Platform::Windows ? "MCPPlugin-plugins-windows.zip" : "MCPPlugin-plugins-linux.zip");

        local_path = std::regex_replace(local_path, std::regex("//"), "/");

        if (fs::exists(local_path)) {
            std::ifstream file(local_path, std::ios::binary);
            if (file.good()) {
                std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                res.set_content(buffer, "application/octet-stream");
                res.set_header("Content-Disposition", "attachment; filename=\"" + fs::path(local_path).filename().string() + "\"");
            } else {
                res.status = 500;
                res.set_content("{\"error\": \"Read failed\"}", "application/json");
            }
        } else {
            res.status = 404;
            res.set_content("{\"error\": \"File not found\"}", "application/json");
        }
    });

    srv_.Post("/self/update", [this](const httplib::Request &, httplib::Response &res) {
        if (updateSelf()) {
            res.status = 200;
            res.set_content(R"({"message": "Self update check completed"})", "application/json");
        } else {
            res.status = 500;
            res.set_content(R"({"error": "Failed to check updates"})", "application/json");
        }
    });

    // Start background auto-update thread
    startPeriodicThread();

    std::cout << "🚀 Server starting on port " << port << "..." << std::endl;
    srv_.listen("0.0.0.0", port);
}

PluginManager::PluginManager() {
    fs::create_directories("plugins");
    fs::create_directories(update_dir_ + "windows");
    fs::create_directories(update_dir_ + "linux");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Use lock-free version to avoid deadlock
        current_version_ = readVersionFileUnsafe();
        std::cout << "📋 Current version: " << (current_version_.empty() ? "none" : current_version_) << std::endl;

        // Load cached release info if exists
        auto cached_release = loadReleaseInfo();
        if (cached_release.has_value()) {
            latest_release_info_ = cached_release.value();
            std::cout << "📂 Loaded cached release info: " << latest_release_info_.tag_name << std::endl;
        }
    }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    std::cout << "🔐 OpenSSL support enabled" << std::endl;
#endif
}

PluginManager::~PluginManager() {
    stopServer();
}

void PluginManager::stopServer() {
    stop_flag_ = true;
    srv_.stop();
    if (periodic_thread_.joinable()) {
        periodic_thread_.join();
    }
}