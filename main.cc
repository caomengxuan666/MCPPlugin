#include "plugin_manager.h"
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
    int port = 6680;
    std::string self_repo_url = "";

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

    const std::string hardcoded_repo_url = "https://github.com/caomengxuan666/MCPPrelugin.git";
    pluginManager.setSelfRepoURL(hardcoded_repo_url);
    std::cout << "Self repository URL (hardcoded): " << hardcoded_repo_url << std::endl;
    std::thread serverThread([&pluginManager, port]() {
        pluginManager.startServer(port);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Server is running on port " << port << std::endl;
    std::cout << "Press Enter to stop the server..." << std::endl;

    std::cin.get();

    std::cout << "Stopping server..." << std::endl;
    pluginManager.stopServer();

    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::cout << "Server stopped." << std::endl;

    return 0;
}
