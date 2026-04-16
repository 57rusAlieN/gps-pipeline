#include "pipeline/MultiFileRecordReader.h"

#include "pipeline/NmeaRecordReader.h"
#include "pipeline/BinaryRecordReader.h"
#include "parser/IBinaryParser.h"  // for RECORD_SIZE constant

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MultiFileRecordReader::MultiFileRecordReader(
    std::vector<fs::path> files,
    std::string           type)
    : m_files{std::move(files)}
    , m_type {std::move(type)}
{
    advanceFile();  // open first file (if any)
}

// ---------------------------------------------------------------------------
// fromDirectory
// ---------------------------------------------------------------------------

MultiFileRecordReader MultiFileRecordReader::fromDirectory(
    const fs::path& root,
    bool            recursive,
    const std::string& type)
{
    std::vector<fs::path> files;

    if (fs::is_regular_file(root))
    {
        files.push_back(root);
    }
    else if (fs::is_directory(root))
    {
        auto collect = [&](const fs::directory_entry& e) {
            if (e.is_regular_file())
                files.push_back(e.path());
        };

        if (recursive)
        {
            for (const auto& e : fs::recursive_directory_iterator(root))
                collect(e);
        }
        else
        {
            for (const auto& e : fs::directory_iterator(root))
                collect(e);
        }

        // Lexicographic order = chronological for YYMMDD/HH/MM layout
        std::sort(files.begin(), files.end());
    }

    return MultiFileRecordReader{std::move(files), type};
}

// ---------------------------------------------------------------------------
// IRecordReader
// ---------------------------------------------------------------------------

bool MultiFileRecordReader::hasNext()
{
    // Drain exhausted readers and advance to the next file.
    while (m_currentReader && !m_currentReader->hasNext())
    {
        ++m_fileIndex;
        if (!advanceFile())
            return false;
    }
    return m_currentReader != nullptr;
}

ParsedRecord MultiFileRecordReader::readNext()
{
    // hasNext() must ensure m_currentReader is valid and has data.
    return m_currentReader->readNext();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool MultiFileRecordReader::advanceFile()
{
    // Find next openable file starting from m_fileIndex.
    while (m_fileIndex < m_files.size())
    {
        const auto& path = m_files[m_fileIndex];

        // Choose open mode: binary files need std::ios::binary so that
        // Windows does not translate \r\n and corrupt record boundaries.
        const bool isBin = (path.extension() == ".bin");
        auto mode = std::ios::in | (isBin ? std::ios::binary : std::ios::openmode{});

        auto stream = std::make_unique<std::ifstream>(path, mode);
        if (!stream->is_open())
        {
            ++m_fileIndex;
            continue;
        }

        m_currentStream = std::move(stream);
        m_currentReader = makeReader(*m_currentStream, path);
        return true;
    }

    m_currentStream.reset();
    m_currentReader.reset();
    return false;
}

std::unique_ptr<IRecordReader> MultiFileRecordReader::makeReader(
    std::ifstream&  stream,
    const fs::path& path) const
{
    // Determine effective type
    std::string eff = m_type;
    if (eff == "auto")
        eff = (path.extension() == ".bin") ? "binary" : "nmea";

    if (eff == "binary")
        return std::make_unique<BinaryRecordReader>(stream);

    // Default: nmea
    return std::make_unique<NmeaRecordReader>(stream);
}
