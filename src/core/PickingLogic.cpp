#include "core/PickingLogic.h"
#include <ctime>
#include <cmath>
#include <cstdio>

namespace PickingLogic {

std::string fmtTimestamp(double t, bool includeSeconds)
{
    std::time_t tt = static_cast<std::time_t>(t);
    if (std::tm* tm = std::gmtime(&tt)) {
        char buf[64];
        std::strftime(buf, sizeof(buf),
            includeSeconds ? "%Y-%m-%d  %H:%M:%S UTC"
                           : "%Y-%m-%d  %H:%M UTC",
            tm);
        return buf;
    }
    return "(invalid)";
}

std::string fmtDuration(double seconds)
{
    if (seconds <= 0.0) return "instant";
    long s = static_cast<long>(seconds);
    if (s < 60)          return std::to_string(s) + "s";
    long m = s / 60; s %= 60;
    if (m < 60)          return std::to_string(m) + "m " + std::to_string(s) + "s";
    long h = m / 60; m %= 60;
    if (h < 24)          return std::to_string(h) + "h " + std::to_string(m) + "m";
    long d = h / 24; h %= 24;
    return std::to_string(d) + "d " + std::to_string(h) + "h";
}

std::string fmtLat(double lat)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f° %c", std::abs(lat), lat >= 0.0 ? 'N' : 'S');
    return buf;
}

std::string fmtLon(double lon)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f° %c", std::abs(lon), lon >= 0.0 ? 'E' : 'W');
    return buf;
}

} // namespace PickingLogic
