#include "output/ConsoleOutput.h"

#include <cmath>
#include <format>

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

    m_os << std::format(
        "{} Coordinates: {:.5f}\u00B0{}, {:.5f}\u00B0{}\n"
        "           Speed: {:.1f} km/h{}, Course: {:.1f}\u00B0\n"
        "           Altitude: {:.1f} m, Satellites: {}, HDOP: {:.1f}\n",
        formatTime(point.time),
        std::abs(point.lat), latDir,
        std::abs(point.lon), lonDir,
        point.speed_kmh, speedNote,
        point.course,
        point.altitude,
        point.satellites,
        point.hdop);
}

// ---------------------------------------------------------------------------
// writeRejected
// ---------------------------------------------------------------------------

void ConsoleOutput::writeRejected(const GpsPoint& point,
                                  const std::string& reason)
{
    m_os << std::format("{} Point rejected: {}\n",
                        formatTime(point.time), reason);
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

    return std::format("[{}:{}:{}]",
                       nmeaTime.substr(0, 2),
                       nmeaTime.substr(2, 2),
                       nmeaTime.substr(4, 2));
}
