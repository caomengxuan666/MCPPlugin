#include "plugin_manager.h"
#include "plugin_repo_manager.h"
#include "env_manager.h"
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
    int port = 6680;
    int repo_port = 6381;
    std::string self_repo_url = "";

    // Load environment variables from .env file
    auto& envManager = EnvManager::getInstance();
    if (envManager.loadFromFile()) {
        std::cout << "✅ Environment file loaded successfully" << std::endl;
    } else {
        std::cout << "⚠️  No environment file found or failed to load" << std::endl;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    std::cerr << "Invalid port number. Using default port 6680." << std::endl;
                    port = 6680;
                }
            }
        } else if (arg == "--repo-port") {
            if (i + 1 < argc) {
                repo_port = std::atoi(argv[++i]);
                if (repo_port <= 0 || repo_port > 65535) {
                    std::cerr << "Invalid repo port number. Using default port 6381." << std::endl;
                    repo_port = 6381;
                }
            }
        } else if (arg.substr(0, 2) != "--" && arg.substr(0, 1) != "-") {
            int temp_port = std::atoi(argv[i]);
            if (temp_port > 0 && temp_port <= 65535) {
                port = temp_port;
            }
        }
    }

    std::cout << "MCP Plugin Server" << std::endl;
    std::cout << "=================" << std::endl;

    auto &pluginManager = PluginManager::getInstance();
    PluginRepoManager repoManager(pluginManager);

    const std::string hardcoded_repo_url = "https://github.com/caomengxuan666/MCPPlugin.git";
    
    pluginManager.setSelfRepoURL(hardcoded_repo_url);
    repoManager.setPluginRepoURL(hardcoded_repo_url);
    
    std::cout << "Self repository URL (hardcoded): " << hardcoded_repo_url << std::endl;

    std::thread serverThread([&pluginManager, port]() {
        pluginManager.startServer(port);
    });

    std::thread repoServerThread([&repoManager, repo_port]() {
        repoManager.startServer(repo_port);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Plugin server is running on port " << port << std::endl;
    std::cout << "Plugin repository server is running on port " << repo_port << std::endl;
    std::cout << "Press Enter to stop the servers..." << std::endl;

    std::cin.get();

    std::cout << "Stopping servers..." << std::endl;
    pluginManager.stopServer();
    repoManager.stopServer();

    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    if (repoServerThread.joinable()) {
        repoServerThread.join();
    }

    std::cout << "Servers stopped." << std::endl;

    return 0;
}