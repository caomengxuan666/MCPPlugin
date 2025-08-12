#pragma once

#include "mcp_plugin.h"
#include <httplib.h>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Platform type enumeration
enum class Platform {
    Windows,
    Linux,
    Unknown
};

// Release asset information (by platform)
struct ReleaseAsset {
    std::string name;        // Asset filename
    std::string download_url;// Download URL
    std::string local_path;  // Local save path
    Platform platform;       // Target platform
};

// Complete release information
struct ReleaseInfo {
    std::string tag_name;            // Version tag (e.g. v1.0.0)
    std::string name;                // Release name
    std::string published_at;        // Publish time
    std::vector<ReleaseAsset> assets;// Assets by platform
};

struct PluginInfo {
    // (Original fields unchanged)
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string url;
    std::string file_path;
    std::vector<ToolInfo> tools;
    std::string release_date;
    bool enabled = true;
    std::vector<std::string> tool_names;
    std::vector<std::string> tool_descriptions;
    std::vector<std::string> tool_parameters;
};

class PluginManager {
public:
    static PluginManager &getInstance() {
        static PluginManager instance;
        return instance;
    }

    // (Original function declarations unchanged)
    bool addPluginFromGitHub(const std::string &github_url);
    const std::vector<PluginInfo> &getPlugins() const;
    const PluginInfo *getPluginById(const std::string &id) const;
    bool removePlugin(const std::string &id);
    void updatePlugins();
    bool updateSelf();
    void setSelfRepoURL(const std::string &url);
    std::string getSelfRepoURL() const;
    void startServer(int port = 8080);
    void stopServer();

    // New: Get latest release info (for user interface)
    std::optional<ReleaseInfo> getLatestReleaseInfo() const;

private:
    PluginManager();
    ~PluginManager();

    // (Original private functions unchanged)
    std::pair<std::string, std::string> parseGitHubURL(const std::string &url) const;
    bool downloadRelease(const std::string &owner, const std::string &repo, const std::string &save_path);
    bool parsePluginManifest(const std::string &plugin_path, PluginInfo &plugin_info);
    void savePlugins() const;
    void loadPlugins();

    // New: Core functionality implementation
    void startPeriodicThread();                                   // Start periodic check thread
    void periodicFetchLatestRelease();                            // Periodic task logic
    std::optional<ReleaseInfo> fetchLatestRelease();              // Get latest release (with platform distinction)
    bool downloadReleaseAsset(const ReleaseAsset &asset);         // Download single asset to local
    Platform getPlatformFromFileName(const std::string &filename);// Determine platform from filename

    httplib::Server srv_;
    std::vector<PluginInfo> plugins_;
    mutable std::mutex mutex_;
    std::string plugins_file_ = "tools.json";
    std::string self_repo_url_;

    // New: Periodic task related
    std::thread periodic_thread_;
    bool stop_flag_ = false;
    ReleaseInfo latest_release_info_;                   // Cache latest release info
    const std::string update_dir_ = "updates/";         // Local storage directory
    std::map<std::string, ReleaseInfo> release_history_;// key: version tag（etc. v1.0.0）
};