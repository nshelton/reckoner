#pragma once

#include <string>
#include <nlohmann/json.hpp>

/// Simple HTTP client for making POST requests to the backend
class HttpClient {
public:
    HttpClient() = default;
    ~HttpClient() = default;

    /// Make a POST request with JSON body
    /// @param url Full URL to POST to
    /// @param json_body JSON object to send as request body
    /// @return JSON response object
    /// @throws std::runtime_error on HTTP errors
    nlohmann::json post(const std::string& url, const nlohmann::json& json_body);

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};
