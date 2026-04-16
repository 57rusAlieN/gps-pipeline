#pragma once

#include "pipeline/IRecordReader.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Composite IRecordReader that processes multiple files in order.
//
// Files can be collected in two ways:
//   1. Explicit list: MultiFileRecordReader(paths, type)
//   2. Directory scan: MultiFileRecordReader::fromDirectory(root, recursive, type)
//
// Files are processed in lexicographic path order, which equals chronological
// order for the YYMMDD/HH/MM.bin directory layout described in BINARY_FORMAT.md.
//
// The "type" parameter selects the per-file reader factory:
//   "auto"   — detect by extension (.bin → binary, else nmea)
//   "nmea"   — force NmeaRecordReader for every file
//   "binary" — force BinaryRecordReader for every file
class MultiFileRecordReader final : public IRecordReader
{
public:
    // Construct from an explicit sorted list of file paths.
    MultiFileRecordReader(std::vector<std::filesystem::path> files,
                          std::string                        type = "auto");

    // Factory: scan a directory and return a ready-to-use reader.
    // If root is a single file, wraps just that file.
    static MultiFileRecordReader fromDirectory(
        const std::filesystem::path& root,
        bool                         recursive,
        const std::string&           type = "auto");

    bool         hasNext() override;
    ParsedRecord readNext() override;

    // Number of files that will be / have been processed.
    std::size_t fileCount() const noexcept { return m_files.size(); }

    // Index of the file currently being read (0-based).
    std::size_t currentFileIndex() const noexcept { return m_fileIndex; }

private:
    std::vector<std::filesystem::path>   m_files;
    std::string                          m_type;
    std::size_t                          m_fileIndex = 0;
    std::unique_ptr<std::ifstream>       m_currentStream;
    std::unique_ptr<IRecordReader>       m_currentReader;

    // Open the next file and create the appropriate reader.
    // Returns false if no more files remain.
    bool advanceFile();

    // Create a reader for the given open stream + file path.
    std::unique_ptr<IRecordReader> makeReader(std::ifstream& stream,
                                              const std::filesystem::path& path) const;
};
