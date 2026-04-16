#include <gtest/gtest.h>

#include "pipeline/MultiFileRecordReader.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ===========================================================================
// Fixtures and helpers
// ===========================================================================

// Minimal valid NMEA pair (same checksums as test_parser.cpp)
static constexpr const char* kNmeaPair =
    "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70\n"
    "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E\n";

// Build a valid 196-byte binary blob
static std::string makeBinBlob(uint8_t navValid = 0,
                                uint64_t datetime = 1706356800ULL)
{
    std::array<uint8_t, 196> buf{};
    std::memcpy(buf.data() +  8, &datetime, 8);
    buf[16] = navValid;
    int32_t lat = 55752057, lon = 37659463;
    std::memcpy(buf.data() + 17, &lat, 4);
    std::memcpy(buf.data() + 21, &lon, 4);
    return std::string(reinterpret_cast<const char*>(buf.data()), 196);
}

class MultiFileTest : public ::testing::Test
{
protected:
    fs::path tmpDir;

    void SetUp() override
    {
        tmpDir = fs::temp_directory_path() / "mfr_test";
        fs::create_directories(tmpDir);
    }
    void TearDown() override { fs::remove_all(tmpDir); }

    // Write text content to a file inside tmpDir
    fs::path writeText(const std::string& name, const std::string& content)
    {
        auto p = tmpDir / name;
        fs::create_directories(p.parent_path());
        std::ofstream f(p);
        f << content;
        return p;
    }

    // Write binary content to a file inside tmpDir
    fs::path writeBin(const std::string& name, const std::string& blob)
    {
        auto p = tmpDir / name;
        fs::create_directories(p.parent_path());
        std::ofstream f(p, std::ios::binary);
        f.write(blob.data(), static_cast<std::streamsize>(blob.size()));
        return p;
    }
};

// ===========================================================================
// fromDirectory — directory scanning
// ===========================================================================

TEST_F(MultiFileTest, EmptyDirectory_FileCountZero)
{
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);
    EXPECT_EQ(reader.fileCount(), 0u);
    EXPECT_FALSE(reader.hasNext());
}

TEST_F(MultiFileTest, SingleNmeaFile_FileCountOne)
{
    writeText("track.nmea", kNmeaPair);
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);
    EXPECT_EQ(reader.fileCount(), 1u);
}

TEST_F(MultiFileTest, SingleBinFile_FileCountOne)
{
    writeBin("01.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);
    EXPECT_EQ(reader.fileCount(), 1u);
}

TEST_F(MultiFileTest, NonRecursive_DoesNotDescend)
{
    writeText("sub/track.nmea", kNmeaPair);   // in subdirectory
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);
    EXPECT_EQ(reader.fileCount(), 0u);  // not found without recursive
}

TEST_F(MultiFileTest, Recursive_FindsFilesInSubdirs)
{
    writeBin("260317/15/01.bin", makeBinBlob());
    writeBin("260317/15/11.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, true);
    EXPECT_EQ(reader.fileCount(), 2u);
}

TEST_F(MultiFileTest, FilesOrderedLexicographically)
{
    // Create in reverse order to ensure sorting is applied
    writeBin("260317/16/56.bin", makeBinBlob(0, 1706356800ULL));
    writeBin("260317/15/01.bin", makeBinBlob(0, 1706353260ULL));  // earlier time
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, true);

    // Should read 15/01.bin first (lexicographic < 16/56.bin)
    ASSERT_TRUE(reader.hasNext());
    auto first = reader.readNext();
    ASSERT_TRUE(first.point.has_value());
    EXPECT_EQ(first.point->time, "150540");  // 1706353260 UTC
}

TEST_F(MultiFileTest, SingleFilePath_WorksAsDirectory)
{
    auto p = writeText("only.nmea", kNmeaPair);
    auto reader = MultiFileRecordReader::fromDirectory(p, false);
    EXPECT_EQ(reader.fileCount(), 1u);
}

// ===========================================================================
// Record delivery
// ===========================================================================

TEST_F(MultiFileTest, NmeaFile_DeliversPoint)
{
    writeText("track.nmea", kNmeaPair);
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);

    bool got = false;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (rec.point.has_value()) { got = true; break; }
    }
    EXPECT_TRUE(got);
}

TEST_F(MultiFileTest, BinaryFile_DeliversPoint)
{
    writeBin("01.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);

    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    ASSERT_TRUE(rec.point.has_value());
    EXPECT_NEAR(rec.point->lat, 55.752057, 1e-5);
}

TEST_F(MultiFileTest, MultipleFiles_AllRecordsDelivered)
{
    writeBin("01.bin", makeBinBlob() + makeBinBlob());
    writeBin("02.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);

    int points = 0;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (rec.point.has_value()) ++points;
    }
    EXPECT_EQ(points, 3);
}

TEST_F(MultiFileTest, CurrentFileIndex_AdvancesCorrectly)
{
    writeBin("a.bin", makeBinBlob());
    writeBin("b.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false);

    EXPECT_EQ(reader.currentFileIndex(), 0u);
    // Read all records from first file
    while (reader.hasNext())
    {
        reader.readNext();
        if (reader.currentFileIndex() >= 1u) break;
    }
    EXPECT_GE(reader.currentFileIndex(), 1u);
}

// ===========================================================================
// Type override
// ===========================================================================

TEST_F(MultiFileTest, TypeAuto_BinExtension_ParsedAsBinary)
{
    writeBin("track.bin", makeBinBlob());
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false, "auto");
    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    EXPECT_TRUE(rec.point.has_value());  // binary parse succeeded
}

TEST_F(MultiFileTest, TypeAuto_NmeaExtension_ParsedAsNmea)
{
    writeText("track.nmea", kNmeaPair);
    auto reader = MultiFileRecordReader::fromDirectory(tmpDir, false, "auto");
    bool got = false;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (rec.point.has_value()) { got = true; break; }
    }
    EXPECT_TRUE(got);
}
