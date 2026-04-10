#pragma once

#include "parser/INmeaParser.h"
#include "filter/IGpsFilter.h"
#include "output/IOutput.h"

#include <string>

// Orchestrates the NMEA processing pipeline:
//   raw line → checksum → parse → filter → output
//
// Dependency injection via constructor; Pipeline does NOT own its collaborators.
class Pipeline
{
public:
    Pipeline(INmeaParser& parser, IGpsFilter& filter, IOutput& output);

    void processLine(const std::string& line);

private:
    INmeaParser& m_parser;
    IGpsFilter&  m_filter;
    IOutput&     m_output;

    // Extract NMEA time field (field 1) from a raw sentence
    static std::string extractTime(const std::string& line);
    // "HHMMSS" → "[HH:MM:SS]"
    static std::string formatTime(const std::string& nmeaTime);
    // True if line is a $GPRMC sentence with status V (no fix)
    static bool isVoidRmc(const std::string& line);
};
