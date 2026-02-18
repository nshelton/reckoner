#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

/// Simple HTTP client for making POST requests to the backend
class HttpClient {
public:
    HttpClient() = default;
    explicit HttpClient(const std::string& api_key) : m_apiKey(api_key) {}
    ~HttpClient() = default;

    /// Set the API key for authenticated requests
    void setApiKey(const std::string& api_key) { m_apiKey = api_key; }

    /// Get the current API key
    const std::string& apiKey() const { return m_apiKey; }

    /// Make a GET request
    /// @param url Full URL to GET
    /// @return JSON response object
    /// @throws std::runtime_error on HTTP errors
    nlohmann::json get(const std::string& url);

    /// Make a POST request with JSON body
    /// @param url Full URL to POST to
    /// @param json_body JSON object to send as request body
    /// @return JSON response object
    /// @throws std::runtime_error on HTTP errors
    nlohmann::json post(const std::string& url, const nlohmann::json& json_body);

    /// Stream a GET response line-by-line (NDJSON)
    /// Sends Accept-Encoding: gzip; curl decompresses on the fly.
    /// @param url Full URL to GET
    /// @param line_callback Called for each complete '\n'-delimited line.
    ///        Return false to cancel the stream early.
    /// @throws std::runtime_error on network errors
    void get_stream(const std::string& url,
                    std::function<bool(const std::string&)> line_callback);

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string m_apiKey;
};
