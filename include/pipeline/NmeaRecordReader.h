#pragma once

#include "pipeline/IRecordReader.h"
#include "parser/NmeaParser.h"

#include <istream>
#include <string>

// Reads NMEA text records from an istream line by line.
//
// Internally owns a NmeaParser (stateful RMC+GGA pairing).
// NMEA-specific logic that lived in Pipeline is encapsulated here:
//   - skip empty lines and comments (#)
//   - skip lines not starting with '$'
//   - XOR checksum validation → error message
//   - void-status $GPRMC → "No valid GPS fix"
//   - waiting-for-pair silence (returns hasNext=true, readNext loops internally)
class NmeaRecordReader final : public IRecordReader
{
public:
    explicit NmeaRecordReader(std::istream& in);

    bool         hasNext() override;
    ParsedRecord readNext() override;

private:
    std::istream& m_in;
    NmeaParser    m_parser;

    // Next line pre-fetched by hasNext(); empty string means EOF seen.
    std::string   m_nextLine;
    bool          m_eof = false;

    // Attempt to produce a ParsedRecord from one line.
    // Returns nullopt when the line should be silently skipped (waiting for
    // RMC+GGA pair, unknown sentence type, etc.).
    std::optional<ParsedRecord> processLine(const std::string& line);

    static std::string extractTime(const std::string& line);
    static std::string formatTime (const std::string& t);
    static bool        isVoidRmc  (const std::string& line);
};
