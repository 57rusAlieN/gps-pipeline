#include "parser/GnssBinaryParser.h"

#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
// Packed binary record layout (docs/BINARY_FORMAT.md)
// All multi-byte integers are little-endian; struct is fully packed.
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
namespace detail {

struct GnssSatInfoBin
{
    uint8_t inView;
    uint8_t inUse;
    uint8_t avgSnr;
    uint8_t maxSnr;
};

struct Vect3dBin { float x, y, z; };

struct AccBin
{
    Vect3dBin lastRaw;
    Vect3dBin lpfLow;
    Vect3dBin lpfHigh;
    uint32_t  timestamp;  // ms since boot
};

struct SatSnrBin
{
    uint8_t data;  // bit 7 = isUse, bits 0-6 = snr (dBHz)
};

struct BinRecord
{
    uint64_t       timeStamp;         // +0    ms since boot
    uint64_t       datetime;          // +8    Unix epoch (seconds UTC)
    uint8_t        navValid;          // +16   NavStatus enum
    int32_t        latitude;          // +17   degrees × 1 000 000
    int32_t        longitude;         // +21   degrees × 1 000 000
    int16_t        height;            // +25   metres above MSL
    uint16_t       speed;             // +27   km/h × 10
    uint16_t       course;            // +29   degrees × 10
    uint8_t        HDOP;              // +31   × 10
    uint8_t        PDOP;              // +32   × 10
    uint8_t        satCount;          // +33
    GnssSatInfoBin satInfo[4];        // +34   4 systems × 4 bytes = 16 bytes
    uint32_t       sysVoltage;        // +50   millivolts
    uint32_t       ignInVoltage;      // +54   millivolts
    AccBin         acc;               // +58   40 bytes
    uint8_t        moveState;         // +98
    uint8_t        availableStates;   // +99
    SatSnrBin      overallArraySnr[96]; // +100  96 bytes
};

static_assert(sizeof(BinRecord) == 196, "BinRecord must be 196 bytes");

} // namespace detail
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Read T from unaligned little-endian buffer (safe on all platforms via memcpy)
template<typename T>
static T readLE(const uint8_t* p)
{
    T v;
    std::memcpy(&v, p, sizeof(T));
    return v;
}

// ---------------------------------------------------------------------------
// IBinaryParser::parseRecord
// ---------------------------------------------------------------------------

std::optional<GpsPoint> GnssBinaryParser::parseRecord(const uint8_t* data,
                                                       std::size_t    size)
{
    if (size != GnssBinaryParser::RECORD_SIZE)
        return std::nullopt;

    // Deserialise via memcpy to respect alignment rules
    detail::BinRecord rec;
    std::memcpy(&rec, data, sizeof(rec));

    // Store nav status before any early return
    m_lastNavStatus = rec.navValid;

    // Only STATUS_VALID (0) yields a usable fix
    if (rec.navValid != 0)
        return std::nullopt;

    GpsPoint pt;
    pt.lat        = rec.latitude  / 1'000'000.0;
    pt.lon        = rec.longitude / 1'000'000.0;
    pt.altitude   = static_cast<double>(rec.height);
    pt.speed_kmh  = rec.speed  / 10.0;
    pt.course     = rec.course / 10.0;
    pt.hdop       = rec.HDOP   / 10.0;
    pt.satellites = rec.satCount;
    pt.time       = epochToTime(rec.datetime);
    pt.stopped    = false;  // StopFilter sets this later

    populateSatellites(pt, data + 100);

    return pt;
}

// ---------------------------------------------------------------------------
// epochToTime — Unix seconds → "HHMMSS"
// ---------------------------------------------------------------------------

std::string GnssBinaryParser::epochToTime(uint64_t unixSeconds)
{
    const std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm gmt{};
#ifdef _WIN32
    gmtime_s(&gmt, &t);
#else
    gmtime_r(&t, &gmt);
#endif
    char buf[7];
    std::snprintf(buf, sizeof(buf), "%02d%02d%02d",
                  gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
    return buf;
}

// ---------------------------------------------------------------------------
// populateSatellites — overallArraySnr[96] → GpsPoint::satellites_in_view
//
// Layout (per docs/BINARY_FORMAT.md):
//   [0..63]  GPS       PRN  = index + 1
//   [64..95] GLONASS   slot = index - 63
//
// Only records with snr > 0 are included.
// ---------------------------------------------------------------------------

void GnssBinaryParser::populateSatellites(GpsPoint& pt, const uint8_t* snrBase)
{
    pt.satellites_in_view.clear();

    for (int i = 0; i < 96; ++i)
    {
        const uint8_t raw   = snrBase[i];
        const uint8_t snr   = raw & 0x7Fu;
        const bool    inUse = (raw & 0x80u) != 0;

        if (snr == 0) continue;  // not tracked — skip

        SatelliteInfo sv;
        sv.snr = snr;

        if (i < 64)
        {
            // GPS
            sv.prn       = i + 1;
            sv.elevation = 0;  // not stored in the binary format
            sv.azimuth   = 0;
        }
        else
        {
            // GLONASS: negative PRN convention to distinguish from GPS
            sv.prn       = -(i - 63);
            sv.elevation = 0;
            sv.azimuth   = 0;
        }
        // Use inUse as a proxy for elevation when no data available
        (void)inUse;

        pt.satellites_in_view.push_back(sv);
    }
}
