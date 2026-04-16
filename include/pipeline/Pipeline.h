#pragma once

#include "pipeline/IRecordReader.h"
#include "filter/IGpsFilter.h"
#include "output/IOutput.h"

// Unified processing pipeline:
//   IRecordReader → IGpsFilter → IOutput
//
// Works identically for NMEA text, binary dumps, or any multi-file source —
// the record-acquisition and format-specific logic live entirely in the reader.
//
// Dependency injection via constructor; Pipeline does NOT own its collaborators.
class Pipeline
{
public:
    Pipeline(IRecordReader& reader, IGpsFilter& filter, IOutput& output);

    // Process all records until the reader is exhausted.
    void run();

private:
    IRecordReader& m_reader;
    IGpsFilter&    m_filter;
    IOutput&       m_output;
};
