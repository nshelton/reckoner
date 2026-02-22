#include "SolarCalculations.h"
#include <cmath>

namespace SolarCalc {

static constexpr double kPi  = 3.14159265358979323846;
static constexpr double kD2R = kPi / 180.0;
static constexpr double kR2D = 180.0 / kPi;

static double normDeg(double d) {
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

double solarAltitudeDeg(double latDeg, double lonDeg, double unixTime)
{
    // Julian Day Number (UT)
    double JD = unixTime / 86400.0 + 2440587.5;

    // Days from J2000.0
    double n = JD - 2451545.0;

    // Mean longitude and mean anomaly (degrees)
    double L = normDeg(280.460 + 0.9856474 * n);
    double g = normDeg(357.528 + 0.9856003 * n);
    double gR = g * kD2R;

    // Ecliptic longitude (degrees)
    double lambda = normDeg(L + 1.915 * std::sin(gR) + 0.020 * std::sin(2.0 * gR));
    double lambdaR = lambda * kD2R;

    // Obliquity of the ecliptic (degrees)
    double eps = 23.439 - 0.0000004 * n;
    double epsR = eps * kD2R;

    // Solar declination (degrees)
    double sinDec = std::sin(epsR) * std::sin(lambdaR);
    double decR   = std::asin(sinDec);

    // Right Ascension (degrees, 0–360)
    double raDeg = normDeg(std::atan2(std::cos(epsR) * std::sin(lambdaR),
                                      std::cos(lambdaR)) * kR2D);

    // Greenwich Mean Sidereal Time (degrees)
    double GMST = normDeg(280.46061837 + 360.98564736629 * n);

    // Local Hour Angle (degrees, normalized to [-180, 180])
    double H = normDeg(GMST + lonDeg - raDeg);
    if (H > 180.0) H -= 360.0;
    double HR = H * kD2R;

    double latR = latDeg * kD2R;

    // Solar altitude
    double sinAlt = std::sin(latR) * std::sin(decR)
                  + std::cos(latR) * std::cos(decR) * std::cos(HR);
    return std::asin(sinAlt) * kR2D;
}

double moonIlluminationFraction(double unixTime)
{
    // Reference new moon: Jan 6, 2000 18:14 UTC (JD 2451549.260)
    // Synodic month: 29.53058867 days
    static constexpr double kNewMoonEpochJD = 2451549.260;
    static constexpr double kSynodicMonth   = 29.53058867;

    double JD       = unixTime / 86400.0 + 2440587.5;
    double daysSince = JD - kNewMoonEpochJD;

    // Phase in [0, 1): 0 = new moon, 0.5 = full moon
    double phase = std::fmod(daysSince / kSynodicMonth, 1.0);
    if (phase < 0.0) phase += 1.0;

    // Illuminated fraction: 0 at new moon, 1 at full moon
    return (1.0 - std::cos(2.0 * kPi * phase)) / 2.0;
}

}  // namespace SolarCalc
