#include "pipeline/NmeaRecordReader.h"

#include "parser/ChecksumValidator.h"

#include <string>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

NmeaRecordReader::NmeaRecordReader(std::istream& in)
    : m_in{in}
{
    // Pre-fetch the first line so hasNext() is meaningful immediately.
    if (!std::getline(m_in, m_nextLine))
        m_eof = true;
}

// ---------------------------------------------------------------------------
// IRecordReader
// ---------------------------------------------------------------------------

bool NmeaRecordReader::hasNext()
{
    // If we already have a buffered line we're not at EOF.
    return !m_eof;
}

ParsedRecord NmeaRecordReader::readNext()
{
    // Loop: consume lines until we produce a ParsedRecord (either a point
    // or a diagnostics error) OR reach EOF.
    while (true)
    {
        const std::string line = std::move(m_nextLine);
        m_nextLine.clear();

        // Advance to the next line
        if (!std::getline(m_in, m_nextLine))
            m_eof = true;

        // Process the current line
        auto maybeRecord = processLine(line);
        if (maybeRecord.has_value())
            return std::move(*maybeRecord);

        // Silent skip — if no more input, return a silent empty record so
        // the caller's hasNext() loop terminates cleanly.
        if (m_eof && m_nextLine.empty())
            return ParsedRecord{std::nullopt, ""};
    }
}

// ---------------------------------------------------------------------------
// Internal per-line processing
// ---------------------------------------------------------------------------

std::optional<ParsedRecord> NmeaRecordReader::processLine(const std::string& line)
{
    // Skip empty lines and comment lines
    if (line.empty() || line[0] == '#')
        return std::nullopt;  // silent skip

    // Only NMEA sentences start with '$'
    if (line[0] != '$')
        return std::nullopt;  // silent skip

    const auto timeStr = extractTime(line);
    const auto timeFmt = formatTime(timeStr);

    if (!ChecksumValidator::validate(line))
        return ParsedRecord{std::nullopt,
                            timeFmt + " Parse error: invalid checksum"};

    auto result = m_parser.parse(line);

    if (!result.has_value())
    {
        if (isVoidRmc(line))
            return ParsedRecord{std::nullopt, timeFmt + " No valid GPS fix"};

        // Waiting for RMC+GGA pair, unknown type, GSV, etc. — silent
        return std::nullopt;
    }

    return ParsedRecord{std::move(result), ""};
}

// ---------------------------------------------------------------------------
// Static helpers (identical to Pipeline helpers)
// ---------------------------------------------------------------------------

std::string NmeaRecordReader::extractTime(const std::string& line)
{
    const auto c1 = line.find(',');
    if (c1 == std::string::npos) return "";
    const auto c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos) return "";
    return line.substr(c1 + 1, c2 - c1 - 1);
}

std::string NmeaRecordReader::formatTime(const std::string& t)
{
    if (t.size() < 6) return "[??:??:??]";
    return "[" + t.substr(0, 2) + ":" + t.substr(2, 2) + ":" + t.substr(4, 2) + "]";
}

bool NmeaRecordReader::isVoidRmc(const std::string& line)
{
    if (line.rfind("$GPRMC", 0) != 0) return false;
    const auto c1 = line.find(',');
    if (c1 == std::string::npos) return false;
    const auto c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;
    return (c2 + 1 < line.size()) && (line[c2 + 1] == 'V');
}
