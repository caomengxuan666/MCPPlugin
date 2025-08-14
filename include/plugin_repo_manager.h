#ifndef PLUGIN_REPO_MANAGER_H
#define PLUGIN_REPO_MANAGER_H

#include "plugin_manager.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

// Try to include miniz for zip operations
#if __has_include("miniz.h")
#include "miniz.h"
#define HAS_MINIZ 1
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

// Filename sanitization function declaration
std::string sanitizeFilename(const std::string& filename);

// Plugin package information
struct PluginPackageInfo {
    std::string id;              // Plugin ID (format: owner_pluginname)
    std::string name;            // Plugin name
    std::string version;         // Plugin version
    std::string description;     // Plugin description
    std::string author;          // Plugin author
    std::vector<ToolInfo> tools; // Plugin tool list
    std::string release_date;    // Release date
    std::string tag_name;        // Associated tag name
    std::string local_path;      // Local file path
};

// Tag information
struct TagInfo {
    std::string tag_name;                      // Tag name
    std::string name;                          // Release name
    std::string published_at;                  // Publish time
    std::vector<ReleaseAsset> assets;          // Asset list
    std::map<std::string, PluginPackageInfo> plugin_packages; // Plugin packages under this tag
};

class PluginRepoManager {
public:
    explicit PluginRepoManager(PluginManager& pluginManager);
    ~PluginRepoManager();

    // Set plugin repository URL
    void setPluginRepoURL(const std::string& url);
    
    // Get plugin repository URL
    std::string getPluginRepoURL() const;
    
    // Update plugin repository information (get all tags)
    bool updateRepoInfo();
    
    // Get all tag information
    std::map<std::string, TagInfo> getAllTags() const;
    
    // Get specific tag information
    std::optional<TagInfo> getTagInfo(const std::string& tag_name) const;
    
    // Download and process all plugins for a specific tag
    bool processTag(const std::string& tag_name);
    
    // Process all tags
    void processAllTags();
    
    // Start periodic scan thread
    void startPeriodicScan(int intervalSeconds = 60);
    
    // Stop periodic scan thread
    void stopPeriodicScan();
    
    // Start HTTP server
    void startServer(int port = 8081);
    
    // Stop HTTP server
    void stopServer();

private:
    // Parse GitHub URL
    std::pair<std::string, std::string> parseGitHubURL(const std::string& url) const;
    
    // Get all release tags for the repository
    std::optional<std::vector<TagInfo>> fetchAllReleases();
    
    // Download asset file
    bool downloadAsset(const ReleaseAsset& asset);
    
    // Extract asset file
    bool extractAsset(const std::string& zip_file, const std::string& extract_dir);
    
    // Process extracted files and repackage plugins
    bool repackagePlugins(const std::string& extract_dir, const std::string& tag_name);
    
    // Determine platform from filename
    Platform getPlatformFromFileName(const std::string& filename);
    
    // Generate plugin ID
    std::string generatePluginId(const std::string& owner, const std::string& plugin_name) const;
    
    // Save tag information
    void saveTagInfo(const TagInfo& tag_info) const;
    
    // Load tag information
    std::optional<TagInfo> loadTagInfo(const std::string& tag_name) const;
    
    // HTTP client setup
    template<typename ClientType>
    void setupHttpClient(ClientType& cli) const;
    
    // Periodic scan task
    void periodicScanTask();
    
    // Check if asset is a plugin package we need
    bool isPluginAsset(const std::string& asset_name) const;

    PluginManager& pluginManager_;
    mutable std::mutex mutex_;
    std::string plugin_repo_url_;           // Plugin repository URL
    std::map<std::string, TagInfo> tags_;   // Tag information cache
    const std::string repo_dir_ = "plugin_repo/";  // Repository local storage directory
    httplib::Server srv_;                   // HTTP server
    
    // Periodic scan related
    std::thread periodic_thread_;
    std::atomic<bool> stop_flag_{false};
    int scan_interval_ = 60;
};

// HTTP client setup template implementation
template<typename ClientType>
void PluginRepoManager::setupHttpClient(ClientType& cli) const {
    cli.set_default_headers({{"User-Agent", "MCPPluginRepoManager"},
                             {"Accept", "application/vnd.github.v3+json"}});

    cli.set_connection_timeout(30);
        
    // Enable following redirects
    cli.set_follow_location(true);

    auto& envManager = EnvManager::getInstance();
    auto token = envManager.get("GITHUB_TOKEN");
    if (token.has_value() && !token.value().empty()) {
        cli.set_bearer_token_auth(token.value().c_str());
    }
}

#endif // PLUGIN_REPO_MANAGER_H