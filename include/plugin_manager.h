#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "env_manager.h"
#include "mcp_plugin.h"
#include <httplib.h>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "env_manager.h"

enum class Platform {
    Unknown,
    Windows,
    Linux
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

    std::string readVersionFileUnsafe() const;

    bool downloadRelease(const std::string &owner, const std::string &repo, const std::string &save_path);
    bool parsePluginManifest(const std::string &plugin_path, PluginInfo &plugin_info);
    std::string readVersionFile() const;
    void writeVersionFile(const std::string &version) const;
    bool isNewerVersion(const std::string &new_tag, const std::string &current_tag) const;
    void savePlugins() const;
    void saveReleaseInfo(const ReleaseInfo &release) const;
    std::optional<ReleaseInfo> loadReleaseInfo() const;
    void loadPlugins();

    // New: Core functionality implementation
    void startPeriodicThread();                                   // Start periodic check thread
    void periodicFetchLatestRelease();                            // Periodic task logic
    std::optional<ReleaseInfo> fetchLatestRelease();              // Get latest release (with platform distinction)
    bool downloadReleaseAsset(const ReleaseAsset &asset);         // Download single asset to local
    Platform getPlatformFromFileName(const std::string &filename);// Determine platform from filename
    // Helper function to set up HTTP client with authentication
    template<typename ClientType>
    void setupHttpClient(ClientType &cli) const;

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
    std::string version_file_ = "latest_version.txt";
    const std::string release_info_file_ = "release_info.json";// Local release info file
    std::string current_version_;
};

// Template implementation for setupHttpClient
template<typename ClientType>
void PluginManager::setupHttpClient(ClientType &cli) const {
    cli.set_default_headers({{"User-Agent", "MCPPluginServer"},
                             {"Accept", "application/vnd.github.v3+json"}});

    cli.set_connection_timeout(30);

    cli.set_follow_location(true);

    auto &envManager = EnvManager::getInstance();
    auto token = envManager.get("GITHUB_TOKEN");// 返回 optional<string>

    if (token.has_value() && !token.value().empty()) {
        cli.set_bearer_token_auth(token.value().c_str());
    }
}

#endif// PLUGIN_MANAGER_H