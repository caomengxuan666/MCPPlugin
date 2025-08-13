#include "env_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

bool EnvManager::loadFromFile(const std::string& env_file) {
    std::ifstream file(env_file);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Find the first '=' character
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue; // Invalid line format
        }

        // Extract key and value
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        // Remove quotes if present
        if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
            if (value.back() == value.front()) {
                value = value.substr(1, value.length() - 2);
            }
        }

        // Store the key-value pair
        env_vars_[key] = value;
    }

    file.close();
    return true;
}

std::optional<std::string> EnvManager::get(const std::string& key) const {
    auto it = env_vars_.find(key);
    if (it != env_vars_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string EnvManager::get(const std::string& key, const std::string& default_value) const {
    auto it = env_vars_.find(key);
    if (it != env_vars_.end()) {
        return it->second;
    }
    return default_value;
}

void EnvManager::set(const std::string& key, const std::string& value) {
    env_vars_[key] = value;
}

bool EnvManager::has(const std::string& key) const {
    return env_vars_.find(key) != env_vars_.end();
}