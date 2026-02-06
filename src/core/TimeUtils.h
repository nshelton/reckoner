#pragma once

#include <string>

/// Utilities for converting between ISO 8601 strings and unix timestamps
namespace TimeUtils {

    /// Parse an ISO 8601 timestamp string to unix timestamp (seconds since epoch)
    /// Format: "YYYY-MM-DDTHH:MM:SSZ" or "YYYY-MM-DDTHH:MM:SS"
    /// @param iso8601 ISO 8601 formatted string
    /// @return Unix timestamp as double (seconds since epoch)
    double parse_iso8601(const std::string& iso8601);

    /// Convert unix timestamp to ISO 8601 string
    /// @param timestamp Unix timestamp (seconds since epoch)
    /// @return ISO 8601 formatted string (UTC)
    std::string to_iso8601(double timestamp);

}
