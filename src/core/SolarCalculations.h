#pragma once

/// Astronomical solar position calculations.
///
/// Uses a simplified NOAA algorithm (Jean Meeus) accurate to ~0.01° for
/// dates within a few centuries of J2000.0.  Sufficient for visualization.
namespace SolarCalc {

/// Returns the solar altitude (elevation) angle in degrees, in the range
/// [-90, 90].  Positive values mean the sun is above the horizon.
///
/// @param latDeg   Observer latitude  in decimal degrees (+N, -S).
/// @param lonDeg   Observer longitude in decimal degrees (+E, -W).
/// @param unixTime UTC time as a Unix timestamp (seconds since 1970-01-01).
double solarAltitudeDeg(double latDeg, double lonDeg, double unixTime);

/// Returns the moon's illuminated fraction in [0, 1].
///   0 = new moon (dark),  1 = full moon (completely lit).
///
/// Phase is global — the same everywhere on Earth.
/// Uses the synodic period and a known new-moon epoch.
///
/// @param unixTime UTC time as a Unix timestamp (seconds since 1970-01-01).
double moonIlluminationFraction(double unixTime);

}  // namespace SolarCalc
