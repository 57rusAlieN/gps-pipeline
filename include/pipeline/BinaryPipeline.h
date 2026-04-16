#pragma once

#include "parser/IBinaryParser.h"
#include "filter/IGpsFilter.h"
#include "output/IOutput.h"

#include <iosfwd>
#include <string>

// Orchestrates the binary GPS processing pipeline:
//   binary stream (196-byte records) → parse → filter → output
//
// Mirrors the design of Pipeline but operates on std::istream in binary mode
// instead of text lines. Partial final records are silently skipped.
//
// Dependency injection via constructor; BinaryPipeline does NOT own collaborators.
class BinaryPipeline
{
public:
    BinaryPipeline(IBinaryParser& parser, IGpsFilter& filter, IOutput& output);

    // Read and process all complete RECORD_SIZE-byte records from `in`.
    void processStream(std::istream& in);

private:
    IBinaryParser& m_parser;
    IGpsFilter&    m_filter;
    IOutput&       m_output;

    // "HHMMSS" → "[HH:MM:SS]"
    static std::string formatTime(const std::string& hhmmss);

    // Convert Unix epoch → "[HH:MM:SS]" directly
    static std::string formatEpoch(const uint8_t* recordBase);

    // Human-readable message for a non-STATUS_VALID navValid byte
    static std::string navErrorMessage(uint8_t navValid, const std::string& timeFmt);
};
