#include "pipeline/BinaryPipeline.h"
#include "parser/GnssBinaryParser.h"  // epochToTime duplicate → use helper below

#include <cstdint>
#include <cstring>
#include <ctime>
#include <istream>
#include <string>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BinaryPipeline::BinaryPipeline(IBinaryParser& parser,
                                IGpsFilter&    filter,
                                IOutput&       output)
    : m_parser{parser}
    , m_filter{filter}
    , m_output{output}
{}

// ---------------------------------------------------------------------------
// processStream
// ---------------------------------------------------------------------------

void BinaryPipeline::processStream(std::istream& in)
{
    constexpr std::size_t RSIZE = IBinaryParser::RECORD_SIZE;
    uint8_t buf[RSIZE];

    while (in.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(RSIZE)))
    {
        // Extract datetime and navValid directly from the raw buffer
        // (known offsets per docs/BINARY_FORMAT.md, little-endian)
        uint64_t epoch;
        std::memcpy(&epoch, buf + 8, 8);
        const uint8_t navValid = buf[16];

        const std::string timeFmt = formatEpoch(buf);

        auto result = m_parser.parseRecord(buf, RSIZE);

        if (!result.has_value())
        {
            m_output.writeError(navErrorMessage(navValid, timeFmt));
            continue;
        }

        GpsPoint& point = *result;
        auto filterResult = m_filter.process(point);

        if (filterResult.status == FilterStatus::Reject)
            m_output.writeRejected(point, filterResult.reason);
        else
            m_output.writePoint(point);
    }
    // Any remaining bytes shorter than RSIZE are silently discarded.
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string BinaryPipeline::formatTime(const std::string& t)
{
    if (t.size() < 6) return "[??:??:??]";
    return "[" + t.substr(0, 2) + ":" + t.substr(2, 2) + ":" + t.substr(4, 2) + "]";
}

std::string BinaryPipeline::formatEpoch(const uint8_t* recordBase)
{
    uint64_t epoch;
    std::memcpy(&epoch, recordBase + 8, 8);

    const std::time_t t = static_cast<std::time_t>(epoch);
    std::tm gmt{};
#ifdef _WIN32
    gmtime_s(&gmt, &t);
#else
    gmtime_r(&t, &gmt);
#endif
    char buf[7];
    std::snprintf(buf, sizeof(buf), "%02d%02d%02d",
                  gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
    return formatTime(buf);
}

std::string BinaryPipeline::navErrorMessage(uint8_t navValid,
                                             const std::string& timeFmt)
{
    std::string reason;
    switch (navValid)
    {
    case 1:    reason = "Receiver error";                   break;
    case 2:    reason = "Cell-tower position (LBS)";        break;
    case 3:    reason = "External location source (EL)";    break;
    case 0xFF: reason = "Last-known position (boot state)"; break;
    default:   reason = "No valid GPS fix";                 break;
    }
    return timeFmt + " " + reason;
}
