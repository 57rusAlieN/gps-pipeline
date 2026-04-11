#include "pipeline/Pipeline.h"

#include "parser/ChecksumValidator.h"
#include "filter/IGpsFilter.h"

Pipeline::Pipeline(INmeaParser& parser, IGpsFilter& filter, IOutput& output)
    : m_parser{parser}
    , m_filter{filter}
    , m_output{output}
{}

void Pipeline::processLine(const std::string& line)
{
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
        return;

    // Only process NMEA sentences
    if (line[0] != '$')
        return;

    const auto timeStr = extractTime(line);
    const auto timeFmt = formatTime(timeStr);

    // Validate checksum before passing to parser
    if (!ChecksumValidator::validate(line))
    {
        m_output.writeError(timeFmt + " Parse error: invalid checksum");
        return;
    }

    // Try to parse the sentence
    auto result = m_parser.parse(line);

    if (!result.has_value())
    {
        // Report "No valid GPS fix" for void-status RMC
        if (isVoidRmc(line))
            m_output.writeError(timeFmt + " No valid GPS fix");
        // Otherwise silent: waiting for pair, unknown type, etc.
        return;
    }

    // Apply filter chain
    GpsPoint& point = *result;
    auto filterResult = m_filter.process(point);

    if (filterResult.status == FilterStatus::Reject)
        m_output.writeRejected(point, filterResult.reason);
    else
        m_output.writePoint(point);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string Pipeline::extractTime(const std::string& line)
{
    // NMEA: $TYPE,TIME,...  → time is between first and second comma
    const auto c1 = line.find(',');
    if (c1 == std::string::npos) return "";
    const auto c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos) return "";
    return line.substr(c1 + 1, c2 - c1 - 1);
}

std::string Pipeline::formatTime(const std::string& t)
{
    if (t.size() < 6) return "[??:??:??]";
    return "[" + t.substr(0, 2) + ":" + t.substr(2, 2) + ":" + t.substr(4, 2) + "]";
}

bool Pipeline::isVoidRmc(const std::string& line)
{
    if (line.rfind("$GPRMC", 0) != 0) return false;

    // Status is field 2: $GPRMC,time,STATUS,...
    const auto c1 = line.find(',');
    if (c1 == std::string::npos) return false;
    const auto c2 = line.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;

    return (c2 + 1 < line.size()) && (line[c2 + 1] == 'V');
}
