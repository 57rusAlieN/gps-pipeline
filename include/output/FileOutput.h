#pragma once

#include "output/IOutput.h"
#include "output/ConsoleOutput.h"

#include <fstream>
#include <memory>
#include <string>

// Writes GPS output to a file, identical format to ConsoleOutput.
//
// Rotation policy (logrotate-style):
//   When the current file exceeds maxSizeKb kilobytes it is renamed to
//   "<path>.1", any existing "<path>.N" files are shifted to "<path>.N+1"
//   (the oldest beyond maxFiles is deleted), and a fresh "<path>" is opened.
//
//   maxSizeKb == 0  → no size limit (no rotation)
//   maxFiles  == 0  → no old-file limit (keep all rotated files)
class FileOutput final : public IOutput
{
public:
    explicit FileOutput(const std::string& path,
                        std::size_t maxSizeKb = 1024,
                        int         maxFiles  = 5);
    ~FileOutput() override;

    void writePoint   (const GpsPoint& point) override;
    void writeRejected(const GpsPoint& point, const std::string& reason) override;
    void writeError   (const std::string& message) override;

    // Path of the currently open file (for inspection in tests)
    const std::string& currentPath() const noexcept { return m_path; }

private:
    std::string                  m_path;
    std::size_t                  m_maxBytes;
    int                          m_maxFiles;
    std::ofstream                m_file;
    std::unique_ptr<ConsoleOutput> m_writer;

    void openFile();
    void checkAndRotate();
    void rotateSuffix();
};
