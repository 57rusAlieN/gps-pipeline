#pragma once

#include "pipeline/IRecordReader.h"
#include "parser/GnssBinaryParser.h"

#include <cstdint>
#include <istream>

// Reads binary GNSS records (196-byte, little-endian) from an istream.
//
// Binary-specific logic that lived in BinaryPipeline is encapsulated here:
//   - reads IBinaryParser::RECORD_SIZE-byte chunks
//   - decodes navValid and formats the appropriate error message
//   - time stamp is extracted directly from the raw buffer for error messages
//   - partial final records (< RECORD_SIZE bytes) are silently discarded
class BinaryRecordReader final : public IRecordReader
{
public:
    explicit BinaryRecordReader(std::istream& in);

    bool         hasNext() override;
    ParsedRecord readNext() override;

private:
    std::istream&    m_in;
    GnssBinaryParser m_parser;

    // Pre-fetched buffer; valid when m_bufferReady == true
    uint8_t m_buf[IBinaryParser::RECORD_SIZE]{};
    bool    m_bufferReady = false;

    // Try to read the next RECORD_SIZE bytes into m_buf.
    // Returns true if successful; false on EOF / partial read.
    bool tryFill();

    static std::string formatEpoch(const uint8_t* recordBase);
    static std::string formatTime (const std::string& hhmmss);
    static std::string navErrorMsg(uint8_t navValid, const std::string& timeFmt);
};
