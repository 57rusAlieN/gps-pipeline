#include "parser/NmeaParser.h"

#include "parser/ChecksumValidator.h"

#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::optional<GpsPoint> NmeaParser::parse(const std::string& line)
{
    if (line.empty() || line[0] != '$')
        return std::nullopt;

    const auto comma = line.find(',');
    if (comma == std::string::npos)
        return std::nullopt;

    const std::string type = line.substr(1, comma - 1);

    if (type == "GPRMC") return parseRmc(line);
    if (type == "GPGGA") return parseGga(line);
    if (type == "GPGSV") { parseGsv(line); return std::nullopt; }

    return std::nullopt;  // unknown / unhandled sentence type
}

// ---------------------------------------------------------------------------
// Private parsers
// ---------------------------------------------------------------------------

std::optional<GpsPoint> NmeaParser::parseRmc(const std::string& sentence)
{
    if (!ChecksumValidator::validate(sentence))
        return std::nullopt;

    const auto star = sentence.find('*');
    const auto fields = split(sentence.substr(1, star - 1), ',');

    // Fields: 0=type 1=time 2=status 3=lat 4=latDir 5=lon 6=lonDir 7=speed 8=course
    if (fields.size() < 9)
        return std::nullopt;

    if (fields[2] != "A")  // V = void / no fix
        return std::nullopt;

    if (fields[1].empty() || fields[3].empty() || fields[4].empty() ||
        fields[5].empty() || fields[6].empty())
        return std::nullopt;

    try
    {
        RmcData rmc;
        rmc.time      = fields[1];
        rmc.lat       = convertCoord(fields[3], fields[4][0]);
        rmc.lon       = convertCoord(fields[5], fields[6][0]);
        rmc.speed_kmh = fields[7].empty() ? 0.0 : knotsToKmh(std::stod(fields[7]));
        rmc.course    = fields[8].empty() ? 0.0 : std::stod(fields[8]);
        m_rmc = rmc;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    return tryEmit();
}

std::optional<GpsPoint> NmeaParser::parseGga(const std::string& sentence)
{
    if (!ChecksumValidator::validate(sentence))
        return std::nullopt;

    const auto star = sentence.find('*');
    const auto fields = split(sentence.substr(1, star - 1), ',');

    // Fields: 0=type 1=time 2=lat 3=latDir 4=lon 5=lonDir 6=quality 7=sats 8=hdop 9=alt
    if (fields.size() < 10)
        return std::nullopt;

    if (fields[1].empty() || fields[2].empty() || fields[3].empty() ||
        fields[4].empty() || fields[5].empty())
        return std::nullopt;

    try
    {
        GgaData gga;
        gga.time       = fields[1];
        gga.lat        = convertCoord(fields[2], fields[3][0]);
        gga.lon        = convertCoord(fields[4], fields[5][0]);
        gga.satellites = fields[7].empty() ? 0 : std::stoi(fields[7]);
        gga.hdop       = fields[8].empty() ? 0.0 : std::stod(fields[8]);
        gga.altitude   = fields[9].empty() ? 0.0 : std::stod(fields[9]);
        m_gga = gga;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    return tryEmit();
}

std::optional<GpsPoint> NmeaParser::tryEmit()
{
    if (!m_rmc || !m_gga)
        return std::nullopt;

    if (m_rmc->time != m_gga->time)
        return std::nullopt;  // timestamp mismatch — keep waiting

    GpsPoint pt;
    pt.time       = m_rmc->time;
    pt.lat        = m_rmc->lat;
    pt.lon        = m_rmc->lon;
    pt.speed_kmh  = m_rmc->speed_kmh;
    pt.course     = m_rmc->course;
    pt.altitude   = m_gga->altitude;
    pt.satellites = m_gga->satellites;
    pt.hdop       = m_gga->hdop;
    // Attach satellite-in-view data collected from GPGSV sentences
    pt.satellites_in_view = std::move(m_pendingSatellites);
    m_pendingSatellites.clear();
    m_gsvTotal    = 0;
    m_gsvReceived = 0;
    // pt.stopped is left false; StopFilter sets it later

    m_rmc.reset();
    m_gga.reset();

    return pt;
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

std::vector<std::string> NmeaParser::split(const std::string& s, char delim)
{
    std::vector<std::string> result;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        result.push_back(std::move(token));
    return result;
}

double NmeaParser::convertCoord(const std::string& value, char direction)
{
    if (value.empty())
        return 0.0;

    const auto dot = value.find('.');
    if (dot == std::string::npos || dot < 2)
        return 0.0;

    // DDMM.MMMM → degrees = value[0 .. dot-3], minutes = value[dot-2 ..]
    const double degrees = std::stod(value.substr(0, dot - 2));
    const double minutes = std::stod(value.substr(dot - 2));
    double result = degrees + minutes / 60.0;

    if (direction == 'S' || direction == 'W')
        result = -result;

    return result;
}

double NmeaParser::knotsToKmh(double knots) noexcept
{
    return knots * 1.852;
}

// ---------------------------------------------------------------------------
// GSV parser — accumulates satellite-in-view data across multiple sentences
// ---------------------------------------------------------------------------
// Format: $GPGSV,totalMsgs,msgNum,totalSvs,PRN1,elev1,az1,snr1,[...],[*cs]
void NmeaParser::parseGsv(const std::string& sentence)
{
    if (!ChecksumValidator::validate(sentence))
        return;

    const auto star   = sentence.find('*');
    const auto fields = split(sentence.substr(1, star == std::string::npos
                                                  ? sentence.size() - 1
                                                  : star - 1), ',');

    // Minimum: type, totalMsgs, msgNum, totalSvs  = 4 fields
    if (fields.size() < 4) return;

    try
    {
        const int total  = std::stoi(fields[1]);
        const int msgNum = std::stoi(fields[2]);

        if (msgNum == 1)
        {
            // First message of a new GSV group — reset accumulator
            m_pendingSatellites.clear();
            m_gsvTotal    = total;
            m_gsvReceived = 0;
        }
        else if (total != m_gsvTotal)
        {
            // Sequence mismatch — discard
            return;
        }

        ++m_gsvReceived;

        // Each satellite block: PRN, elevation, azimuth, SNR (fields 4..N, groups of 4)
        for (std::size_t i = 4; i + 3 < fields.size(); i += 4)
        {
            // SNR may be empty (satellite tracked but no signal)
            const std::string& prnStr = fields[i];
            if (prnStr.empty()) break;

            SatelliteInfo sv;
            sv.prn       = std::stoi(prnStr);
            sv.elevation = fields[i+1].empty() ? 0 : std::stoi(fields[i+1]);
            sv.azimuth   = fields[i+2].empty() ? 0 : std::stoi(fields[i+2]);
            sv.snr       = fields[i+3].empty() ? 0 : std::stoi(fields[i+3]);
            m_pendingSatellites.push_back(sv);
        }
    }
    catch (const std::exception&) { /* ignore malformed GSV */ }
}
