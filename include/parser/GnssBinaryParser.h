#pragma once

#include "parser/IBinaryParser.h"

#include <cstdint>
#include <string>

// NavStatus values as defined in docs/BINARY_FORMAT.md
enum class NavStatus : uint8_t
{
    STATUS_VALID = 0,   // Valid GPS fix
    RCVR_ERR     = 1,   // Receiver error
    LBS          = 2,   // Cell-tower positioning
    EL           = 3,   // External location source
    FIRST_START  = 0xFF // Boot state / last-known position
};

// Parses a 196-byte GnssBinaryDump record (little-endian, packed) into a GpsPoint.
//
// Returns nullopt when:
//   - size != IBinaryParser::RECORD_SIZE (196)
//   - navValid != STATUS_VALID (0)
//
// Conversions applied (per docs/BINARY_FORMAT.md):
//   latitude  / longitude  ÷ 1 000 000  → decimal degrees
//   speed                  ÷ 10         → km/h
//   course                 ÷ 10         → degrees
//   HDOP                   ÷ 10         → DOP value
//   height                              → metres (direct)
//   datetime (Unix epoch)               → GpsPoint::time  "HHMMSS"
//
// satellites_in_view is populated from overallArraySnr[96]:
//   indices  0..63  → GPS  (PRN = index + 1)
//   indices 64..95  → GLONASS (slot = index - 63)
//   Only entries with snr > 0 are included.
class GnssBinaryParser final : public IBinaryParser
{
public:
    std::optional<GpsPoint> parseRecord(const uint8_t* data,
                                        std::size_t    size) override;

    uint8_t lastNavStatus() const noexcept override { return m_lastNavStatus; }

private:
    uint8_t m_lastNavStatus = 0;

    // Convert Unix-epoch seconds to "HHMMSS" string (UTC)
    static std::string epochToTime(uint64_t unixSeconds);

    // Build satellites_in_view from overallArraySnr[96] at data+100
    static void populateSatellites(GpsPoint& pt, const uint8_t* snrBase);
};
