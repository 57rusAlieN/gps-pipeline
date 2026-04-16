#pragma once

#include "GpsPoint.h"

#include <optional>
#include <string>

// Result of reading one logical record from any data source.
//
// When point has a value the record was parsed successfully and is ready
// for the filter chain.
//
// When point is empty the source produced a diagnostics-only event (no fix,
// receiver error, etc.) and error holds the fully formatted message that
// should be passed directly to IOutput::writeError.
struct ParsedRecord
{
    std::optional<GpsPoint> point;
    std::string             error;  // non-empty when !point
};

// Abstract source of parsed GPS records.
//
// Implementations wrap a concrete parser (NmeaParser / GnssBinaryParser) and
// own the I/O logic required to feed it (reading text lines, binary chunks, …).
// The unified Pipeline operates exclusively through this interface.
class IRecordReader
{
public:
    // Returns false when the stream is exhausted and no more records exist.
    virtual bool hasNext() = 0;

    // Reads and returns the next record.
    // Precondition: hasNext() == true.
    virtual ParsedRecord readNext() = 0;

    virtual ~IRecordReader() = default;
};
