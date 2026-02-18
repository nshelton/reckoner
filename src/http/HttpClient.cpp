#include "HttpClient.h"
#include <curl/curl.h>
#include <stdexcept>
#include <iostream>

size_t HttpClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

namespace {
    struct StreamContext {
        std::string buffer;
        std::function<bool(const std::string&)> callback;
        bool stop = false;
    };
}

size_t HttpClient::stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* ctx = static_cast<StreamContext*>(userp);

    ctx->buffer.append(static_cast<char*>(contents), total);

    size_t pos;
    while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
        std::string line = ctx->buffer.substr(0, pos);
        ctx->buffer.erase(0, pos + 1);
        if (!line.empty()) {
            if (!ctx->callback(line)) {
                ctx->stop = true;
                return 0;  // Signals curl to abort (CURLE_WRITE_ERROR)
            }
        }
    }

    return total;
}

void HttpClient::get_stream(const std::string& url,
                             std::function<bool(const std::string&)> line_callback) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize CURL");

    StreamContext ctx;
    ctx.callback = std::move(line_callback);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");  // Auto-decompress gzip
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);            // 5 min for large datasets

    struct curl_slist* headers = nullptr;
    if (!m_apiKey.empty()) {
        std::string auth_header = "X-API-Key: " + m_apiKey;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);

    // CURLE_WRITE_ERROR is expected when the callback returns false (early cancel)
    if (res != CURLE_OK && res != CURLE_WRITE_ERROR) {
        std::string err = "CURL stream failed: ";
        err += curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error(err);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && (http_code < 200 || http_code >= 300)) {
        throw std::runtime_error("HTTP stream failed with code " + std::to_string(http_code));
    }

    // Flush any remaining content in the buffer (final line with no trailing newline)
    if (!ctx.buffer.empty() && !ctx.stop) {
        ctx.callback(ctx.buffer);
    }
}

nlohmann::json HttpClient::get(const std::string& url) {
    std::cout << "GET: " << url << std::endl;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_string;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::string error_msg = "CURL request failed: ";
        error_msg += curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error(error_msg);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error("HTTP request failed with code " + std::to_string(http_code));
    }

    try {
        return nlohmann::json::parse(response_string);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("Failed to parse JSON response: ") + e.what());
    }
}

nlohmann::json HttpClient::post(const std::string& url, const nlohmann::json& json_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_string;
    std::string request_body = json_body.dump();

    // Set up the request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body.size());

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Add API key if present
    if (!m_apiKey.empty()) {
        std::string auth_header = "X-API-Key: " + m_apiKey;
        headers = curl_slist_append(headers, auth_header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set up response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Clean up
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::string error_msg = "CURL request failed: ";
        error_msg += curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error(error_msg);
    }

    // Check HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        std::cerr << "HTTP " << http_code << " response: " << response_string << std::endl;
        std::cerr << "Request body: " << request_body << std::endl;
        throw std::runtime_error("HTTP request failed with code " + std::to_string(http_code));
    }

    // Parse JSON response
    try {
        return nlohmann::json::parse(response_string);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("Failed to parse JSON response: ") + e.what());
    }
}
