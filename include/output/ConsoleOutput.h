#pragma once

#include "output/IOutput.h"

#include <ostream>

// Writes formatted GPS data to any std::ostream (stdout, file, stringstream).
// Output format follows the ТЗ specification:
//   writePoint:    [HH:MM:SS] Coordinates: …  Speed: … km/h, …
//   writeRejected: [HH:MM:SS] Point rejected: <reason>
//   writeError:    <message> (passed through verbatim)
class ConsoleOutput final : public IOutput
{
public:
    explicit ConsoleOutput(std::ostream& os);

    void writePoint   (const GpsPoint& point) override;
    void writeRejected(const GpsPoint& point, const std::string& reason) override;
    void writeError   (const std::string& message) override;

private:
    std::ostream& m_os;

    // Converts NMEA time "HHMMSS[.ss]" → "[HH:MM:SS]"
    static std::string formatTime(const std::string& nmeaTime);
};
