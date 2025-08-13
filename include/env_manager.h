#ifndef ENV_MANAGER_H
#define ENV_MANAGER_H

#include <string>
#include <unordered_map>
#include <optional>

class EnvManager {
public:
    static EnvManager& getInstance() {
        static EnvManager instance;
        return instance;
    }

    /**
     * @brief Load environment variables from a file
     * @param env_file Path to the environment file
     * @return true if loaded successfully, false otherwise
     */
    bool loadFromFile(const std::string& env_file = ".env");

    /**
     * @brief Get environment variable value by key
     * @param key Environment variable name
     * @return Optional containing the value if found, nullopt otherwise
     */
    std::optional<std::string> get(const std::string& key) const;

    /**
     * @brief Get environment variable value by key with a default value
     * @param key Environment variable name
     * @param default_value Default value to return if key not found
     * @return Environment variable value or default value
     */
    std::string get(const std::string& key, const std::string& default_value) const;

    /**
     * @brief Set environment variable
     * @param key Environment variable name
     * @param value Environment variable value
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Check if an environment variable exists
     * @param key Environment variable name
     * @return true if exists, false otherwise
     */
    bool has(const std::string& key) const;

private:
    EnvManager() = default;
    ~EnvManager() = default;
    
    // Remove copy constructor and assignment operator
    EnvManager(const EnvManager&) = delete;
    EnvManager& operator=(const EnvManager&) = delete;

    std::unordered_map<std::string, std::string> env_vars_;
};

#endif // ENV_MANAGER_H