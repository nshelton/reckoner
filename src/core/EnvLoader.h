#pragma once

#include <string>
#include <unordered_map>

/// Simple .env file loader
class EnvLoader {
public:
    /// Load environment variables from a .env file
    /// @param filepath Path to .env file
    /// @return Map of key-value pairs
    static std::unordered_map<std::string, std::string> load(const std::string& filepath);

    /// Get a specific value from env map, or return default if not found
    static std::string get(
        const std::unordered_map<std::string, std::string>& env,
        const std::string& key,
        const std::string& default_value = ""
    );
};
