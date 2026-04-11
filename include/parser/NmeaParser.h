#pragma once

#include "GpsPoint.h"
#include "parser/INmeaParser.h"

#include <optional>
#include <string>
#include <vector>

class NmeaParser : public INmeaParser
{
public:
    std::optional<GpsPoint> parse(const std::string& line) override;

private:
    // Intermediate data from a single RMC sentence
    struct RmcData
    {
        std::string time;
        double lat       = 0.0;
        double lon       = 0.0;
        double speed_kmh = 0.0;
        double course    = 0.0;
    };

    // Intermediate data from a single GGA sentence
    struct GgaData
    {
        std::string time;
        double lat       = 0.0;
        double lon       = 0.0;
        int    satellites = 0;
        double hdop      = 0.0;
        double altitude  = 0.0;
    };

    std::optional<RmcData> m_rmc;
    std::optional<GgaData> m_gga;

    // Satellite-in-view data accumulated from GPGSV sentences
    std::vector<SatelliteInfo> m_pendingSatellites;
    int m_gsvTotal    = 0;  // total GSV messages expected
    int m_gsvReceived = 0;  // messages received so far

    // Parse individual sentence types; each validates its own checksum
    std::optional<GpsPoint> parseRmc(const std::string& sentence);
    std::optional<GpsPoint> parseGga(const std::string& sentence);
    void                    parseGsv(const std::string& sentence);

    // Emit a GpsPoint if both m_rmc and m_gga are set with matching time;
    // resets both on success.
    std::optional<GpsPoint> tryEmit();

    // Utilities
    static std::vector<std::string> split(const std::string& s, char delim);

    // Convert NMEA coordinate (DDMM.MMMM or DDDMM.MMMM) to decimal degrees.
    // direction: 'N'/'E' → positive, 'S'/'W' → negative.
    static double convertCoord(const std::string& value, char direction);

    static double knotsToKmh(double knots) noexcept;
};
