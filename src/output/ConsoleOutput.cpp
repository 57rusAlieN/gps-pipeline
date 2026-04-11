#include "output/ConsoleOutput.h"

#include <cmath>
#include <iomanip>

ConsoleOutput::ConsoleOutput(std::ostream& os)
    : m_os{os}
{}

// ---------------------------------------------------------------------------
// writePoint — main formatted output line
// ---------------------------------------------------------------------------
//
// Format (from ТЗ):
//   [12:00:00] Coordinates: 55.75206°N, 37.65946°E
//              Speed: 46.3 km/h, Course: 45.0°
//              Altitude: 150.0 m, Satellites: 10, HDOP: 0.8
//
// Stopped point appends " (stopped)" after speed.

void ConsoleOutput::writePoint(const GpsPoint& point)
{
    const char latDir = point.lat >= 0.0 ? 'N' : 'S';
    const char lonDir = point.lon >= 0.0 ? 'E' : 'W';

    const std::string speedNote = point.stopped ? " (stopped)" : "";

    m_os << std::fixed
         << formatTime(point.time)
         << " Coordinates: "
         << std::setprecision(5) << std::abs(point.lat) << "\u00B0" << latDir
         << ", " << std::abs(point.lon) << "\u00B0" << lonDir << '\n'
         << "           Speed: "
         << std::setprecision(1) << point.speed_kmh << " km/h" << speedNote
         << ", Course: " << point.course << "\u00B0\n"
         << "           Altitude: " << point.altitude << " m"
         << ", Satellites: " << point.satellites
         << ", HDOP: " << point.hdop << '\n';
}

// ---------------------------------------------------------------------------
// writeRejected
// ---------------------------------------------------------------------------

void ConsoleOutput::writeRejected(const GpsPoint& point,
                                  const std::string& reason)
{
    m_os << formatTime(point.time) << " Point rejected: " << reason << '\n';
}

// ---------------------------------------------------------------------------
// writeError — pass-through; caller provides full message with timestamp
// ---------------------------------------------------------------------------

void ConsoleOutput::writeError(const std::string& message)
{
    m_os << message << '\n';
}

// ---------------------------------------------------------------------------
// formatTime — "HHMMSS[.ss]" → "[HH:MM:SS]"
// ---------------------------------------------------------------------------

std::string ConsoleOutput::formatTime(const std::string& nmeaTime)
{
    if (nmeaTime.size() < 6)
        return "[??:??:??]";

    return "[" + nmeaTime.substr(0, 2) + ":" +
                  nmeaTime.substr(2, 2) + ":" +
                  nmeaTime.substr(4, 2) + "]";
}
