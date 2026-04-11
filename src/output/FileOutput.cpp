#include "output/FileOutput.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

FileOutput::FileOutput(const std::string& path,
                       std::size_t maxSizeKb,
                       int         maxFiles)
    : m_path{path}
    , m_maxBytes{maxSizeKb * 1024}
    , m_maxFiles{maxFiles}
{
    openFile();
}

FileOutput::~FileOutput()
{
    // ConsoleOutput must be destroyed before m_file is closed
    m_writer.reset();
}

// ---------------------------------------------------------------------------
// IOutput
// ---------------------------------------------------------------------------

void FileOutput::writePoint(const GpsPoint& point)
{
    m_writer->writePoint(point);
    m_file.flush();
    checkAndRotate();
}

void FileOutput::writeRejected(const GpsPoint& point, const std::string& reason)
{
    m_writer->writeRejected(point, reason);
    m_file.flush();
    checkAndRotate();
}

void FileOutput::writeError(const std::string& message)
{
    m_writer->writeError(message);
    m_file.flush();
    checkAndRotate();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FileOutput::openFile()
{
    m_file.open(m_path, std::ios::out | std::ios::app);
    if (!m_file.is_open())
        throw std::runtime_error("FileOutput: cannot open '" + m_path + "'");

    m_writer = std::make_unique<ConsoleOutput>(m_file);
}

void FileOutput::checkAndRotate()
{
    if (m_maxBytes == 0) return;

    std::error_code ec;
    const auto size = fs::file_size(m_path, ec);
    if (ec || size < m_maxBytes) return;

    // Destroy writer before closing the stream it references
    m_writer.reset();
    m_file.close();
    rotateSuffix();
    openFile();
}

void FileOutput::rotateSuffix()
{
    if (m_maxFiles <= 0)
    {
        // No old-file limit: just shift everything
        for (int i = 999; i >= 1; --i)
        {
            const auto from = m_path + "." + std::to_string(i);
            const auto to   = m_path + "." + std::to_string(i + 1);
            if (fs::exists(from))
                fs::rename(from, to);
        }
    }
    else
    {
        // Remove oldest beyond the limit
        const auto oldest = m_path + "." + std::to_string(m_maxFiles);
        if (fs::exists(oldest))
            fs::remove(oldest);

        // Shift: .N-1 → .N, ..., .1 → .2
        for (int i = m_maxFiles - 1; i >= 1; --i)
        {
            const auto from = m_path + "." + std::to_string(i);
            const auto to   = m_path + "." + std::to_string(i + 1);
            if (fs::exists(from))
                fs::rename(from, to);
        }
    }

    // Rename the current file to .1
    fs::rename(m_path, m_path + ".1");
}
