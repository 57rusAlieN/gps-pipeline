#include "pipeline/BinaryRecordReader.h"

#include <cstring>
#include <ctime>
#include <istream>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BinaryRecordReader::BinaryRecordReader(std::istream& in)
    : m_in{in}
{
    // Pre-fetch the first record so hasNext() is meaningful immediately.
    m_bufferReady = tryFill();
}

// ---------------------------------------------------------------------------
// IRecordReader
// ---------------------------------------------------------------------------

bool BinaryRecordReader::hasNext()
{
    return m_bufferReady;
}

ParsedRecord BinaryRecordReader::readNext()
{
    // Consume pre-fetched buffer, then attempt to load the next one.
    const std::string timeFmt = formatEpoch(m_buf);
    const uint8_t     navValid = m_buf[16];

    auto result = m_parser.parseRecord(m_buf, GnssBinaryParser::RECORD_SIZE);

    // Advance to next record
    m_bufferReady = tryFill();

    if (!result.has_value())
        return ParsedRecord{std::nullopt, navErrorMsg(navValid, timeFmt)};

    return ParsedRecord{std::move(result), ""};
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool BinaryRecordReader::tryFill()
{
    return static_cast<bool>(
        m_in.read(reinterpret_cast<char*>(m_buf),
                  static_cast<std::streamsize>(GnssBinaryParser::RECORD_SIZE)));
}

std::string BinaryRecordReader::formatTime(const std::string& t)
{
    if (t.size() < 6) return "[??:??:??]";
    return "[" + t.substr(0, 2) + ":" + t.substr(2, 2) + ":" + t.substr(4, 2) + "]";
}

std::string BinaryRecordReader::formatEpoch(const uint8_t* recordBase)
{
    uint64_t epoch{};
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

std::string BinaryRecordReader::navErrorMsg(uint8_t navValid,
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
