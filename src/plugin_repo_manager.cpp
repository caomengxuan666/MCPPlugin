#include "plugin_repo_manager.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <chrono>
#include <thread>
#include <future>

namespace fs = std::filesystem;
using json = nlohmann::json;

/**
 * @brief Safely remove a directory, even if files are in use.
 * 
 * This function will attempt to delete multiple times and output detailed error information on failure.
 * If a file is in use, it will wait for a period of time before retrying.
 * 
 * @param path The path of the directory to delete
 * @param max_retries Maximum number of retries (default 3 times)
 * @param retry_delay_ms Delay between each retry (milliseconds, default 500ms)
 * @return true if deletion is successful, false if deletion fails
 */
bool safeRemoveAll(const fs::path& path, int max_retries = 3, int retry_delay_ms = 500) {
    if (path.empty() || !fs::exists(path)) {
        return true; // Path is empty or does not exist, considered successful
    }

    std::error_code ec;
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        if (attempt > 1) {
            std::cout << "🔄 Retrying to remove directory '" << path << "' (attempt " << attempt << "/" << max_retries << ")..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
        } else {
            std::cout << "🗑️  Removing directory: " << path << std::endl;
        }

        // Clear previous error code
        ec.clear();

        try {
            // Try to delete
            uintmax_t removed_count = fs::remove_all(path, ec);
            
            if (ec) {
                std::cerr << "❌ Error removing directory '" << path 
                          << "': " << ec.message() << " (Code: " << ec.value() << ")" << std::endl;
                // If it's a permissions issue or access denied, it may be that a file is in use
                if (ec.value() == 5 || ec.value() == 32) { // 5=Access Denied, 32=Sharing Violation (Windows)
                    std::cerr << "   💡 Hint: A file might be locked by another process (e.g., antivirus, file explorer)." << std::endl;
                }
            } else {
                std::cout << "✅ Successfully removed " << removed_count << " items from '" << path << "'" << std::endl;
                return true; // Deletion successful
            }

        } catch (const std::exception& ex) {
            std::cerr << "❌ Exception during removal of '" << path 
                      << "': " << ex.what() << std::endl;
            ec = std::make_error_code(std::errc::io_error); // Mark as failed
        }

        // If not the last attempt, wait and retry
        if (attempt < max_retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
        }
    }

    // All attempts failed
    std::cerr << "🛑 Failed to remove directory after " << max_retries << " attempts: " << path << std::endl;
    return false;
}

// Add filename sanitization function
std::string sanitizeFilename(const std::string& filename) {
    std::string safe;
    for (char c : filename) {
        // Allow alphanumeric characters and some safe special characters
        if (std::isalnum(static_cast<unsigned char>(c)) || 
            c == '.' || c == '-' || c == '_' || c == ' ') {
            safe += c;
        } 
        // Replace path separators with underscores
        else if (c == '/' || c == '\\') {
            safe += '_';
        }
        // Replace other special characters with underscores
        else {
            safe += '_';
        }
    }
    
    // Limit filename length to prevent it from being too long
    if (safe.length() > 255) {
        // Preserve file extension
        size_t dot_pos = safe.find_last_of('.');
        if (dot_pos != std::string::npos && dot_pos > 0) {
            std::string extension = safe.substr(dot_pos);
            std::string name = safe.substr(0, dot_pos);
            if (name.length() > (255 - extension.length())) {
                name = name.substr(0, 255 - extension.length());
            }
            safe = name + extension;
        } else {
            safe = safe.substr(0, 255);
        }
    }
    
    // Make sure filename is not empty
    if (safe.empty()) {
        safe = "unnamed_file";
    }
    
    return safe;
}

PluginRepoManager::PluginRepoManager(PluginManager& pluginManager) 
    : pluginManager_(pluginManager) {
    
    fs::create_directories(repo_dir_);
    
    // Note: updateRepoInfo() is not called in the constructor because the plugin repository URL has not been set yet
    // updateRepoInfo() will be called in setPluginRepoURL(), or can be called manually
    
    // Start periodic scan task, check for updates every 15 min
    startPeriodicScan(900);
}

PluginRepoManager::~PluginRepoManager() {
    stopPeriodicScan();
    stopServer();
}

void PluginRepoManager::setPluginRepoURL(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        plugin_repo_url_ = url;
    }
    
    // After URL is set, update repository information
    // Note: updateRepoInfo is no longer called within the lock to avoid deadlock
    updateRepoInfo();
}

std::string PluginRepoManager::getPluginRepoURL() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugin_repo_url_;
}

std::pair<std::string, std::string> PluginRepoManager::parseGitHubURL(const std::string& url) const {
    std::regex github_regex(R"(https?://github\.com/([^/]+)/([^/]+?)(?:\.git)?)");
    std::smatch matches;
    
    if (std::regex_match(url, matches, github_regex) && matches.size() > 2) {
        return {matches[1].str(), matches[2].str()};
    }
    
    return {"", ""};
}

std::optional<std::vector<TagInfo>> PluginRepoManager::fetchAllReleases() {
    auto [owner, repo] = parseGitHubURL(plugin_repo_url_);
    if (owner.empty() || repo.empty()) {
        std::cerr << "Invalid GitHub URL: " << plugin_repo_url_ << std::endl;
        return std::nullopt;
    }
    
    // Clean owner and repo names
    std::string safe_owner = sanitizeFilename(owner);
    std::string safe_repo = sanitizeFilename(repo);
    
    if (safe_owner.empty() || safe_repo.empty()) {
        std::cerr << "Invalid owner or repo name after sanitization" << std::endl;
        return std::nullopt;
    }
    
    httplib::Client cli("https://api.github.com");
    setupHttpClient(cli);
    
    std::string path = "/repos/" + safe_owner + "/" + safe_repo + "/releases";
    
    // Check path length
    if (path.length() > 200) {
        std::cerr << "API path too long: " << path << std::endl;
        return std::nullopt;
    }
    
    auto res = cli.Get(path.c_str());
    
    if (!res) {
        std::cerr << "Failed to fetch releases from: " << path << std::endl;
        return std::nullopt;
    }
    
    if (res->status != 200) {
        std::cerr << "GitHub API error: " << res->status << " - " << res->body << std::endl;
        return std::nullopt;
    }
    
    try {
        json releases_json = json::parse(res->body);
        std::vector<TagInfo> tags;
        
        for (const auto& release_json : releases_json) {
            TagInfo tag_info;
            tag_info.tag_name = release_json.value("tag_name", "");
            tag_info.name = release_json.value("name", "");
            tag_info.published_at = release_json.value("published_at", "");
            
            // Clean tag name
            tag_info.tag_name = sanitizeFilename(tag_info.tag_name);
            tag_info.name = sanitizeFilename(tag_info.name);
            
            if (tag_info.tag_name.empty()) {
                continue;
            }
            
            // Parse assets - use safe string operations
            if (release_json.contains("assets") && release_json["assets"].is_array()) {
                for (const auto& asset_json : release_json["assets"]) {
                    ReleaseAsset asset;
                    asset.name = asset_json.value("name", "");
                    asset.download_url = asset_json.value("browser_download_url", "");
                    
                    // Clean asset name
                    asset.name = sanitizeFilename(asset.name);
                    
                    if (isPluginAsset(asset.name)) {
                        asset.platform = getPlatformFromFileName(asset.name);
                        // Construct local path: plugin_repo/{tag_name}/{asset_name}
                        asset.local_path = repo_dir_ + tag_info.tag_name + "/" + asset.name;
                        
                        // Check path length
                        if (asset.local_path.length() > 260) {
                            std::cerr << "Asset local path too long, skipping: " << asset.local_path << std::endl;
                            continue;
                        }
                        
                        tag_info.assets.push_back(std::move(asset));
                    }
                }
            }
            
            tags.push_back(std::move(tag_info));
        }
        
        return tags;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse releases JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool PluginRepoManager::updateRepoInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (plugin_repo_url_.empty()) {
        std::cerr << "Plugin repository URL not set" << std::endl;
        return false;
    }
    
    auto releases_opt = fetchAllReleases();
    if (!releases_opt.has_value()) {
        std::cerr << "Failed to fetch releases from repository" << std::endl;
        return false;
    }
    
    auto& releases = releases_opt.value();
    
    // Update tag cache
    tags_.clear();
    for (auto& tag : releases) {
        // Check if full information for this tag already exists
        auto existing_tag = loadTagInfo(tag.tag_name);
        if (existing_tag.has_value() && !existing_tag->plugin_packages.empty()) {
            // Use existing full tag information
            tags_[tag.tag_name] = std::move(existing_tag.value());
            std::cout << "Loaded existing info for tag: " << tag.tag_name << std::endl;
        } else {
            // Use basic information obtained from API
            tags_[tag.tag_name] = std::move(tag);
        }
    }
    
    std::cout << "Repository info updated, found " << tags_.size() << " tags" << std::endl;
    return true;
}

std::map<std::string, TagInfo> PluginRepoManager::getAllTags() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tags_;
}

std::optional<TagInfo> PluginRepoManager::getTagInfo(const std::string& tag_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tags_.find(tag_name);
    if (it != tags_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PluginRepoManager::processTag(const std::string& tag_name) {
    // Clean tag name
    std::string safe_tag_name = sanitizeFilename(tag_name);
    if (safe_tag_name.empty()) {
        std::cerr << "Invalid tag name: " << tag_name << std::endl;
        return false;
    }
    
    // Get tag information but don't hold the lock
    std::optional<TagInfo> tag_info_opt;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto tag_it = tags_.find(safe_tag_name);
        if (tag_it == tags_.end()) {
            std::cerr << "Tag " << safe_tag_name << " not found" << std::endl;
            return false;
        }
        
        // Check if the tag has already been processed (has plugin packages)
        if (!tag_it->second.plugin_packages.empty()) {
            std::cout << "Tag " << safe_tag_name << " already processed, skipping..." << std::endl;
            return true;
        }
        
        tag_info_opt = tag_it->second;
    }
    
    TagInfo& tag_info = tag_info_opt.value();
    
    std::cout << "Processing tag: " << safe_tag_name << " with " << tag_info.assets.size() << " assets" << std::endl;
    
    // Create tag directory, using cleaned tag name
    std::string tag_dir = repo_dir_ + safe_tag_name;
    
    // Check path length
    if (tag_dir.length() > 200) {  // Leave some space for subdirectories and files
        std::cerr << "Tag directory path too long: " << tag_dir << std::endl;
        return false;
    }
    
    fs::create_directories(tag_dir);
    
    bool processed = false;
    
    // Download and process each asset in parallel
    std::vector<std::future<std::pair<bool, ReleaseAsset>>> download_futures;
    std::vector<ReleaseAsset> successful_downloads;
    
    // Start all downloads in parallel
    for (const auto& asset : tag_info.assets) {
        std::cout << "Starting download for asset: " << asset.name << std::endl;
        
        // Check asset local path length
        if (asset.local_path.length() > 260) {
            std::cerr << "Asset local path too long, skipping: " << asset.local_path << std::endl;
            continue;
        }
        
        // Launch download in a separate thread
        auto future = std::async(std::launch::async, [this, asset]() -> std::pair<bool, ReleaseAsset> {
            bool success = this->downloadAsset(asset);
            return std::make_pair(success, asset);
        });
        
        download_futures.push_back(std::move(future));
    }
    
    // Wait for all downloads to complete and collect successful ones
    for (size_t i = 0; i < download_futures.size(); ++i) {
        try {
            auto result = download_futures[i].get();
            if (result.first) {
                std::cout << "Successfully downloaded asset: " << result.second.name << std::endl;
                successful_downloads.push_back(result.second);
            } else {
                std::cerr << "Failed to download asset: " << result.second.name << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during download: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Download phase completed. Successful downloads: " << successful_downloads.size() 
              << "/" << tag_info.assets.size() << std::endl;
    
    // Process successfully downloaded assets
    for (const auto& asset : successful_downloads) {
        // Extract asset
        std::string extract_dir = tag_dir + "/temp_extract";
        
        // Check extract directory path length
        if (extract_dir.length() > 230) {  // Leave some space for internal files
            std::cerr << "Extract directory path too long: " << extract_dir << std::endl;
            safeRemoveAll(extract_dir);
            continue;
        }
        
        fs::create_directories(extract_dir);
        
        if (!extractAsset(asset.local_path, extract_dir)) {
            std::cerr << "Failed to extract asset: " << asset.name << std::endl;
            safeRemoveAll(extract_dir);
            continue;
        }
        
        // Repackage plugins
        if (!repackagePlugins(extract_dir, safe_tag_name)) {
            std::cerr << "Failed to repackage plugins from asset: " << asset.name << std::endl;
        } else {
            processed = true;
        }
        
        // Clean up extract directory
        safeRemoveAll(extract_dir);
    }
    
    // If processing is successful, save tag information
    if (processed) {
        // Update tag information
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tags_[safe_tag_name] = tag_info;
        }
        
        saveTagInfo(tag_info);
        std::cout << "Successfully processed tag: " << safe_tag_name << std::endl;
    } else {
        std::cout << "No plugins processed for tag: " << safe_tag_name << std::endl;
    }
    
    return processed;
}

void PluginRepoManager::processAllTags() {
    std::cout << "Processing all tags..." << std::endl;
    
    // Get tag list but don't hold the lock, avoid deadlock when calling processTag
    std::vector<std::string> tag_names;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "Found " << tags_.size() << " tags to process" << std::endl;
        
        for (const auto& pair : tags_) {
            tag_names.push_back(pair.first);
        }
    }
    
    // Process each tag outside the lock
    int processed_count = 0;
    for (const auto& tag_name : tag_names) {
        std::cout << "Processing tag: " << tag_name << std::endl;
        if (processTag(tag_name)) {
            processed_count++;
        }
    }
    
    std::cout << "Finished processing tags. Successfully processed: " << processed_count 
              << "/" << tag_names.size() << std::endl;
}

bool PluginRepoManager::downloadAsset(const ReleaseAsset& asset) {
    const int max_retries = 3;
    const int retry_delay = 5; // seconds
    
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        if (attempt > 1) {
            std::cout << "Retry attempt " << attempt << "/" << max_retries << " for asset: " << asset.name << std::endl;
            // Wait for a period of time before retrying
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
        }
        
        // Check if file already exists
        if (fs::exists(asset.local_path)) {
            std::cout << "Asset already downloaded: " << asset.name << std::endl;
            return true;
        }
        
        std::cout << "Attempting to download asset from URL: " << asset.download_url << std::endl;
        
        httplib::Client cli("https://github.com");
        setupHttpClient(cli);

        // Enable following redirects
        cli.set_follow_location(true);
        
        // Set timeouts for connection and reading
        cli.set_connection_timeout(10, 0); // 10 seconds
        cli.set_read_timeout(30, 0);       // 30 seconds
        cli.set_write_timeout(10, 0);      // 10 seconds
        
        std::cout << "Downloading asset: " << asset.name << std::endl;
        
        // Variables for progress tracking
        size_t current_size = 0;
        size_t total_size = 0;
        bool progress_started = false;
        
        // Progress bar helper function
        auto show_progress = [](size_t current, size_t total) {
            if (total == 0) return;
            
            int progress_percent = static_cast<int>((static_cast<double>(current) / total) * 100);
            int bar_width = 50;
            int filled_width = static_cast<int>((static_cast<double>(current) / total) * bar_width);
            
            std::cout << "\r[";
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled_width) {
                    std::cout << "=";
                } else if (i == filled_width) {
                    std::cout << ">";
                } else {
                    std::cout << " ";
                }
            }
            std::cout << "] " << progress_percent << "% (" << current << "/" << total << " bytes)";
            std::cout.flush();
        };
        
        std::string response_body;
        // Use the correct signature for httplib::Client::Get
        auto res = cli.Get(asset.download_url.c_str(),
            // Content receiver callback
            [&](const char* data, size_t data_length) -> bool {
                response_body.append(data, data_length);
                current_size += data_length;
                if (progress_started && total_size > 0) {
                    show_progress(current_size, total_size);
                }
                return true; // Continue receiving
            },
            // Progress callback
            [&](size_t current, size_t total) -> bool {
                total_size = total;
                progress_started = true;
                std::cout << "\n📦 File size: " << total << " bytes" << std::endl;
                return true; // Continue receiving
            });
        
        if (!res) {
            std::cerr << "\nFailed to download asset: " << asset.name << std::endl;
            std::cerr << "Error: " << res.error() << std::endl;
            continue; // Retry
        }
        
        std::cout << "\nDownload completed with status: " << res->status << std::endl;
        
        if (res->status != 200) {
            std::cerr << "\nFailed to download asset: " << asset.name 
                      << ", status: " << res->status << std::endl;
            
            // Output response headers for debugging
            std::cerr << "Response headers:" << std::endl;
            for (const auto& header : res->headers) {
                std::cerr << "  " << header.first << ": " << header.second << std::endl;
            }
            
            // If it's a 404 error, no need to retry
            if (res->status == 404) {
                std::cerr << "Asset not found, skipping retries." << std::endl;
                return false;
            }
            
            // If there's a response body, also output part of it for debugging
            if (!res->body.empty()) {
                std::cerr << "Response body (first 500 chars): " << res->body.substr(0, 500) << std::endl;
            }
            
            continue; // Retry (unless it's 404)
        }
        
        // Clear progress bar
        if (progress_started && total_size > 0) {
            std::cout << std::endl;
        }
        
        // Create directories if needed
        fs::create_directories(fs::path(asset.local_path).parent_path());
        
        // Write to file
        std::ofstream file(asset.local_path, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot create file: " << asset.local_path << std::endl;
            continue; // Retry
        }
        
        file << response_body;
        file.close();
        
        std::cout << "Successfully downloaded: " << asset.name << " to " << asset.local_path << std::endl;
        return true;
    }
    
    std::cerr << "Failed to download asset after " << max_retries << " attempts: " << asset.name << std::endl;
    return false;
}

bool PluginRepoManager::extractAsset(const std::string& zip_file, const std::string& extract_dir) {
#if HAS_MINIZ
    // Check if zip file exists
    if (!fs::exists(zip_file)) {
        std::cerr << "Zip file does not exist: " << zip_file << std::endl;
        return false;
    }
    
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    
    // Make sure zip file path is safe
    if (zip_file.length() > 260) {  // Windows MAX_PATH
        std::cerr << "Zip file path too long: " << zip_file << std::endl;
        return false;
    }
    
    if (!mz_zip_reader_init_file(&zip_archive, zip_file.c_str(), 0)) {
        std::cerr << "Failed to initialize zip reader for: " << zip_file << std::endl;
        return false;
    }
    
    int file_count = (int)mz_zip_reader_get_num_files(&zip_archive);
    bool success = true;
    
    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            std::cerr << "Failed to get file stat for entry " << i << std::endl;
            success = false;
            continue;
        }
        
        // Clean and validate filename
        std::string safe_filename = sanitizeFilename(std::string(file_stat.m_filename));
        if (safe_filename.empty()) {
            std::cerr << "Skipping empty filename in zip" << std::endl;
            continue;
        }
        
        // Safely construct output file path
        std::string output_file = extract_dir + "/" + safe_filename;
        
        // Check path length
        if (output_file.length() > 260) {  // Windows MAX_PATH
            std::cerr << "Output file path too long, skipping: " << output_file << std::endl;
            continue;
        }
        
        fs::create_directories(fs::path(output_file).parent_path());
        
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            fs::create_directories(output_file);
        } else {
            // Ensure the output directory exists
            fs::create_directories(fs::path(output_file).parent_path());
            
            if (!mz_zip_reader_extract_to_file(&zip_archive, i, output_file.c_str(), 0)) {
                std::cerr << "Failed to extract file: " << safe_filename << std::endl;
                success = false;
            }
        }
    }
    
    // Always finalize the zip reader
    mz_zip_reader_end(&zip_archive);
    return success;
#else
    std::cerr << "Miniz not available, cannot extract zip file: " << zip_file << std::endl;
    return false;
#endif
}

bool PluginRepoManager::repackagePlugins(const std::string& extract_dir, const std::string& tag_name) {
#if HAS_MINIZ
    // Find plugin files (dll or so) and corresponding JSON files
    for (const auto& entry : fs::directory_iterator(extract_dir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::string extension = entry.path().extension().string();
            
            // Check if it's a plugin file (dll or so)
            if (extension == ".dll" || extension == ".so") {
                std::string plugin_name = entry.path().stem().string();
                std::string json_filename = plugin_name + "_tools.json";
                std::string json_path = extract_dir + "/" + json_filename;
                
                // Clean filenames to prevent buffer overflow
                std::string safe_plugin_name = sanitizeFilename(plugin_name);
                std::string safe_filename = sanitizeFilename(filename);
                std::string safe_json_filename = sanitizeFilename(json_filename);
                
                // Check cleaned filenames
                if (safe_plugin_name.empty() || safe_filename.empty() || safe_json_filename.empty()) {
                    std::cerr << "Skipping invalid filename" << std::endl;
                    continue;
                }
                
                // Rebuild paths
                json_path = extract_dir + "/" + safe_json_filename;
                
                // Check if corresponding JSON file exists
                if (fs::exists(json_path)) {
                    try {
                        // Create platform directory
                        std::string platform_dir = (extension == ".dll") ? "windows" : "linux";
                        std::string output_dir = repo_dir_ + tag_name + "/" + platform_dir;
                        
                        // Check path length
                        if (output_dir.length() > 200) {  // Leave some space for filenames
                            std::cerr << "Output directory path too long, skipping: " << output_dir << std::endl;
                            continue;
                        }
                        
                        fs::create_directories(output_dir);
                        
                        // Generate package filename: {plugin_name}_{tag_name}_{timestamp}.zip
                        auto now = std::chrono::system_clock::now();
                        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch()).count();
                        
                        std::string package_name = safe_plugin_name + "_" + tag_name + "_" + 
                                                 std::to_string(timestamp) + ".zip";
                        
                        // Check package name length
                        if (package_name.length() > 255) {
                            std::cerr << "Package name too long, skipping: " << package_name << std::endl;
                            continue;
                        }
                        
                        std::string package_path = output_dir + "/" + package_name;
                        
                        // Check full path length
                        if (package_path.length() > 260) {  // Windows MAX_PATH
                            std::cerr << "Package path too long, skipping: " << package_path << std::endl;
                            continue;
                        }
                        
                        // Create zip file
                        std::unique_ptr<mz_zip_archive> zip_archive(new mz_zip_archive);
                        memset(zip_archive.get(), 0, sizeof(mz_zip_archive));
                        
                        if (!mz_zip_writer_init_file(zip_archive.get(), package_path.c_str(), 0)) {
                            std::cerr << "Failed to initialize zip writer for: " << package_path << std::endl;
                            continue;
                        }
                        
                        // Add plugin file - fix function call parameters
                        if (!mz_zip_writer_add_file(zip_archive.get(), safe_filename.c_str(), 
                                                   entry.path().string().c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
                            std::cerr << "Failed to add plugin file to zip: " << safe_filename << std::endl;
                            mz_zip_writer_end(zip_archive.get());
                            continue;
                        }
                        
                        // Add JSON file - fix function call parameters
                        if (!mz_zip_writer_add_file(zip_archive.get(), safe_json_filename.c_str(), 
                                                   json_path.c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
                            std::cerr << "Failed to add JSON file to zip: " << safe_json_filename << std::endl;
                            mz_zip_writer_end(zip_archive.get());
                            continue;
                        }
                        
                        if (!mz_zip_writer_finalize_archive(zip_archive.get())) {
                            std::cerr << "Failed to finalize zip archive: " << package_path << std::endl;
                            mz_zip_writer_end(zip_archive.get());
                            continue;
                        }
                        
                        mz_zip_writer_end(zip_archive.get());
                        zip_archive.reset(); // Explicitly reset smart pointer
                        
                        std::cout << "Created plugin package: " << package_path << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Exception occurred while repackaging plugin: " << e.what() << std::endl;
                        return false;
                    }
                } else {
                    std::cerr << "JSON file not found for plugin: " << safe_plugin_name << std::endl;
                }
            }
        }
    }
    
    return true;
#else
    std::cerr << "Miniz not available, cannot repackage plugins" << std::endl;
    return false;
#endif
}

Platform PluginRepoManager::getPlatformFromFileName(const std::string& filename) {
    std::string lower_filename = filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);
    
    if (lower_filename.find("windows") != std::string::npos || 
        lower_filename.find(".dll") != std::string::npos) {
        return Platform::Windows;
    } else if (lower_filename.find("linux") != std::string::npos || 
               lower_filename.find(".so") != std::string::npos) {
        return Platform::Linux;
    } else {
        return Platform::Unknown;
    }
}

std::string PluginRepoManager::generatePluginId(const std::string& owner, const std::string& plugin_name) const {
    return owner + "_" + plugin_name;
}

void PluginRepoManager::saveTagInfo(const TagInfo& tag_info) const {
    // Clean tag name to ensure safe filename
    std::string safe_tag_name = sanitizeFilename(tag_info.tag_name);
    if (safe_tag_name.empty()) {
        std::cerr << "Invalid tag name for saving: " << tag_info.tag_name << std::endl;
        return;
    }
    
    std::string tag_file = repo_dir_ + safe_tag_name + ".json";
    
    // Check file path length
    if (tag_file.length() > 260) {  // Windows MAX_PATH
        std::cerr << "Tag file path too long: " << tag_file << std::endl;
        return;
    }
    
    try {
        json j;
        
        j["tag_name"] = safe_tag_name;
        j["name"] = sanitizeFilename(tag_info.name);
        j["published_at"] = tag_info.published_at;
        
        // Safely handle assets array
        json assets_json = json::array();
        for (const auto& asset : tag_info.assets) {
            // Clean asset name
            std::string safe_asset_name = sanitizeFilename(asset.name);
            if (safe_asset_name.empty()) {
                continue;
            }
            
            json asset_json;
            asset_json["name"] = safe_asset_name;
            asset_json["download_url"] = asset.download_url;
            
            // Clean local path
            std::string safe_local_path = sanitizeFilename(asset.local_path);
            asset_json["local_path"] = safe_local_path;
            
            asset_json["platform"] = (asset.platform == Platform::Windows) ? "windows" : 
                                    (asset.platform == Platform::Linux) ? "linux" : "unknown";
            assets_json.push_back(std::move(asset_json));
        }
        j["assets"] = std::move(assets_json);
        
        // Safely handle plugin_packages object
        json plugins_json = json::object();
        for (const auto& pair : tag_info.plugin_packages) {
            // Clean plugin ID
            std::string safe_plugin_id = sanitizeFilename(pair.first);
            if (safe_plugin_id.empty()) {
                continue;
            }
            
            const auto& plugin = pair.second;
            json plugin_json;
            plugin_json["id"] = safe_plugin_id;
            plugin_json["name"] = sanitizeFilename(plugin.name);
            plugin_json["version"] = sanitizeFilename(plugin.version);
            plugin_json["description"] = sanitizeFilename(plugin.description);
            plugin_json["author"] = sanitizeFilename(plugin.author);
            plugin_json["release_date"] = plugin.release_date;
            plugin_json["tag_name"] = sanitizeFilename(plugin.tag_name);
            
            // Clean local path
            std::string safe_plugin_local_path = sanitizeFilename(plugin.local_path);
            plugin_json["local_path"] = safe_plugin_local_path;
            
            plugins_json[safe_plugin_id] = std::move(plugin_json);
        }
        j["plugin_packages"] = std::move(plugins_json);
        
        // Safely write to file
        std::ofstream file(tag_file);
        if (file) {
            file << j.dump(2);
            file.close();
            std::cout << "Saved tag info to: " << tag_file << std::endl;
        } else {
            std::cerr << "Failed to save tag info to: " << tag_file << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred while saving tag info: " << e.what() << std::endl;
    }
}

std::optional<TagInfo> PluginRepoManager::loadTagInfo(const std::string& tag_name) const {
    // Clean tag name to ensure safe filename
    std::string safe_tag_name = sanitizeFilename(tag_name);
    if (safe_tag_name.empty()) {
        std::cerr << "Invalid tag name for loading: " << tag_name << std::endl;
        return std::nullopt;
    }
    
    std::string tag_file = repo_dir_ + safe_tag_name + ".json";
    
    // Check file path length
    if (tag_file.length() > 260) {  // Windows MAX_PATH
        std::cerr << "Tag file path too long: " << tag_file << std::endl;
        return std::nullopt;
    }
    
    if (!fs::exists(tag_file)) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(tag_file);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        json j;
        file >> j;
        file.close();
        
        TagInfo tag_info;
        tag_info.tag_name = j.value("tag_name", "");
        tag_info.name = j.value("name", "");
        tag_info.published_at = j.value("published_at", "");
        
        // Clean tag information
        tag_info.tag_name = sanitizeFilename(tag_info.tag_name);
        tag_info.name = sanitizeFilename(tag_info.name);
        
        // Safely handle assets
        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset_json : j["assets"]) {
                ReleaseAsset asset;
                asset.name = asset_json.value("name", "");
                asset.download_url = asset_json.value("download_url", "");
                asset.local_path = asset_json.value("local_path", "");
                std::string platform = asset_json.value("platform", "unknown");
                asset.platform = (platform == "windows") ? Platform::Windows : 
                                (platform == "linux") ? Platform::Linux : Platform::Unknown;
                
                // Clean asset information
                asset.name = sanitizeFilename(asset.name);
                asset.local_path = sanitizeFilename(asset.local_path);
                
                if (!asset.name.empty()) {
                    tag_info.assets.push_back(std::move(asset));
                }
            }
        }
        
        // Safely handle plugin_packages
        if (j.contains("plugin_packages") && j["plugin_packages"].is_object()) {
            for (auto it = j["plugin_packages"].begin(); it != j["plugin_packages"].end(); ++it) {
                std::string plugin_id = it.key();
                // Clean plugin ID
                plugin_id = sanitizeFilename(plugin_id);
                if (plugin_id.empty()) {
                    continue;
                }
                
                PluginPackageInfo plugin;
                const auto& plugin_json = it.value();
                plugin.id = plugin_id;
                plugin.name = plugin_json.value("name", "");
                plugin.version = plugin_json.value("version", "");
                plugin.description = plugin_json.value("description", "");
                plugin.author = plugin_json.value("author", "");
                plugin.release_date = plugin_json.value("release_date", "");
                plugin.tag_name = plugin_json.value("tag_name", "");
                plugin.local_path = plugin_json.value("local_path", "");
                
                // Clean plugin information
                plugin.name = sanitizeFilename(plugin.name);
                plugin.version = sanitizeFilename(plugin.version);
                plugin.description = sanitizeFilename(plugin.description);
                plugin.author = sanitizeFilename(plugin.author);
                plugin.tag_name = sanitizeFilename(plugin.tag_name);
                plugin.local_path = sanitizeFilename(plugin.local_path);
                
                if (!plugin.id.empty()) {
                    tag_info.plugin_packages[plugin.id] = std::move(plugin);
                }
            }
        }
        
        return tag_info;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load tag info from " << tag_file << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool PluginRepoManager::isPluginAsset(const std::string& asset_name) const {
    // Check if asset name contains plugin-related keywords
    std::string lower_name = asset_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    // Only match plugin packages, not server packages
    // Match plugin package naming rules - contains "plugin" and ".zip", but does not contain "server"
    if (lower_name.find("plugin") != std::string::npos && 
        lower_name.find(".zip") != std::string::npos &&
        lower_name.find("server") == std::string::npos) {
        
        // Check if it's a platform-specific plugin package or a generic plugin package
        if (lower_name.find("windows") != std::string::npos || 
            lower_name.find("linux") != std::string::npos ||
            (lower_name.find("windows") == std::string::npos && 
             lower_name.find("linux") == std::string::npos)) {
            return true;
        }
    }
    
    return false;
}

void PluginRepoManager::startPeriodicScan(int intervalSeconds) {
    scan_interval_ = intervalSeconds;
    stop_flag_.store(false);
    
    periodic_thread_ = std::thread([this]() {
        while (!stop_flag_.load()) {
            periodicScanTask();
            
            // Sleep for the specified interval
            for (int i = 0; i < scan_interval_ && !stop_flag_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    });
    
    std::cout << "Started periodic scan thread with interval: " << intervalSeconds << " seconds" << std::endl;
}

void PluginRepoManager::stopPeriodicScan() {
    stop_flag_.store(true);
    
    if (periodic_thread_.joinable()) {
        periodic_thread_.join();
    }
    
    std::cout << "Stopped periodic scan thread" << std::endl;
}

void PluginRepoManager::periodicScanTask() {
    std::cout << "Running periodic scan..." << std::endl;
    
    // Update repository information
    if (updateRepoInfo()) {
        // Process all unprocessed tags
        processAllTags();
    }
}

void PluginRepoManager::startServer(int port) {
    std::cout << "🚀 Plugin repository server starting on port " << port << "..." << std::endl;
    
    // Start processing all tags immediately after startup
    std::thread processing_thread([]() {
        // Give the server some time to start
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "Starting initial tag processing..." << std::endl;
    });
    processing_thread.detach();
    
    // Get list of all tags
    srv_.Get("/tags", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            json response = json::array();
            
            for (const auto& pair : tags_) {
                response.push_back(pair.first);
            }
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /tags endpoint: " << e.what() << std::endl;
        }
    });
    
    // Get specific tag information
    srv_.Get(R"(/tags/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string tag_name = req.matches[1];
            auto tag_info = getTagInfo(tag_name);
            
            if (!tag_info.has_value()) {
                res.status = 404;
                res.set_content(R"({"error": "Tag not found"})", "application/json");
                return;
            }
            
            json response;
            response["tag_name"] = tag_info->tag_name;
            response["name"] = tag_info->name;
            response["published_at"] = tag_info->published_at;
            
            json plugins = json::array();
            for (const auto& plugin_pair : tag_info->plugin_packages) {
                const auto& plugin = plugin_pair.second;
                json plugin_json;
                plugin_json["id"] = plugin.id;
                plugin_json["name"] = plugin.name;
                plugin_json["version"] = plugin.version;
                plugin_json["description"] = plugin.description;
                plugin_json["author"] = plugin.author;
                plugin_json["release_date"] = plugin.release_date;
                plugin_json["tag_name"] = plugin.tag_name;
                plugin_json["local_path"] = plugin.local_path;
                plugins.push_back(std::move(plugin_json));
            }
            response["plugin_packages"] = std::move(plugins);
            
            json assets = json::array();
            for (const auto& asset : tag_info->assets) {
                json asset_json;
                asset_json["name"] = asset.name;
                asset_json["download_url"] = asset.download_url;
                asset_json["local_path"] = asset.local_path;
                asset_json["platform"] = (asset.platform == Platform::Windows) ? "windows" : 
                                       (asset.platform == Platform::Linux) ? "linux" : "unknown";
                assets.push_back(std::move(asset_json));
            }
            response["assets"] = std::move(assets);
            
            res.set_content(response.dump(2), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /tags/{tag} endpoint: " << e.what() << std::endl;
        }
    });
    
    // Trigger processing of a specific tag
    srv_.Post(R"(/tags/([^/]+)/process)", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string tag_name = req.matches[1];
            
            if (processTag(tag_name)) {
                res.status = 200;
                res.set_content(R"({"message": "Tag processed successfully"})", "application/json");
            } else {
                res.status = 500;
                res.set_content(R"({"error": "Failed to process tag"})", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /tags/{tag}/process endpoint: " << e.what() << std::endl;
        }
    });
    
    // Download plugin package
    srv_.Get(R"(/download/([^/]+)/([^/]+)/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string tag_name = req.matches[1];
            std::string platform = req.matches[2];
            std::string package_name = req.matches[3];
            
            std::string package_path = repo_dir_ + tag_name + "/" + platform + "/" + package_name;
            
            if (!fs::exists(package_path)) {
                res.status = 404;
                res.set_content(R"({"error": "Plugin package not found"})", "application/json");
                return;
            }
            
            std::ifstream file(package_path, std::ios::binary);
            if (!file.good()) {
                res.status = 500;
                res.set_content(R"({"error": "Failed to read plugin package"})", "application/json");
                return;
            }
            
            // Use a safe way to read file content
            std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            
            res.set_content(buffer, "application/octet-stream");
            res.set_header("Content-Disposition", "attachment; filename=\"" + package_name + "\"");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /download endpoint: " << e.what() << std::endl;
        }
    });
    
    // Start periodic scan
    srv_.Post("/scan/start", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            startPeriodicScan();
            res.status = 200;
            res.set_content(R"({"message": "Periodic scan started"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /scan/start endpoint: " << e.what() << std::endl;
        }
    });
    
    // Stop periodic scan
    srv_.Post("/scan/stop", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            stopPeriodicScan();
            res.status = 200;
            res.set_content(R"({"message": "Periodic scan stopped"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error": "Internal server error"})", "application/json");
            std::cerr << "Exception in /scan/stop endpoint: " << e.what() << std::endl;
        }
    });
    
    srv_.listen("0.0.0.0", port);
}

void PluginRepoManager::stopServer() {
    srv_.stop();
}