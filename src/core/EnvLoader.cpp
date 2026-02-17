#include "EnvLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::unordered_map<std::string, std::string> EnvLoader::load(const std::string& filepath) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Warning: Could not open env file: " << filepath << std::endl;
        return env;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the '=' separator
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        // Extract key and value
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        // Remove quotes if present
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        env[key] = value;
    }

    return env;
}

std::string EnvLoader::get(
    const std::unordered_map<std::string, std::string>& env,
    const std::string& key,
    const std::string& default_value
) {
    auto it = env.find(key);
    if (it != env.end()) {
        return it->second;
    }
    return default_value;
}
