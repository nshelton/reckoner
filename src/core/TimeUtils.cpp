#include "TimeUtils.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace TimeUtils {

double parse_iso8601(const std::string& iso8601) {
    std::tm tm = {};
    std::istringstream ss(iso8601);

    // Try parsing with 'Z' suffix
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        throw std::runtime_error("Failed to parse ISO 8601 timestamp: " + iso8601);
    }

    // Convert to unix timestamp (assumes UTC)
    #ifdef _WIN32
        return static_cast<double>(_mkgmtime(&tm));
    #else
        return static_cast<double>(timegm(&tm));
    #endif
}

std::string to_iso8601(double timestamp) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::gmtime(&t);

    if (!tm) {
        throw std::runtime_error("Failed to convert timestamp to ISO 8601");
    }

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return std::string(buf);
}

}
