#include <gtest/gtest.h>

#include "output/FileOutput.h"
#include "GpsPoint.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper — read the full content of a file
// ---------------------------------------------------------------------------
static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

static GpsPoint makePoint(const std::string& time = "120000")
{
    GpsPoint p;
    p.time       = time;
    p.lat        = 55.752057;
    p.lon        = 37.659463;
    p.speed_kmh  = 46.3;
    p.course     = 45.0;
    p.altitude   = 150.0;
    p.satellites = 10;
    p.hdop       = 0.8;
    return p;
}

// ---------------------------------------------------------------------------
// Fixture — creates and removes a temp directory for test files
// ---------------------------------------------------------------------------
class FileOutputTest : public ::testing::Test
{
protected:
    fs::path tmpDir;

    void SetUp() override
    {
        tmpDir = fs::temp_directory_path() / "gps_pipeline_test";
        fs::create_directories(tmpDir);
    }

    void TearDown() override
    {
        fs::remove_all(tmpDir);
    }

    std::string tmpFile(const std::string& name) const
    {
        return (tmpDir / name).string();
    }
};

// ===========================================================================
// Basic write
// ===========================================================================

TEST_F(FileOutputTest, WritesPointToFile)
{
    auto path = tmpFile("out.log");
    {
        FileOutput fo(path, 0, 0);  // no rotation
        fo.writePoint(makePoint());
    }
    auto content = readFile(path);
    EXPECT_NE(content.find("[12:00:00]"), std::string::npos);
    EXPECT_NE(content.find("55.75206"), std::string::npos);
}

TEST_F(FileOutputTest, WritesRejectedToFile)
{
    auto path = tmpFile("out.log");
    {
        FileOutput fo(path, 0, 0);
        fo.writeRejected(makePoint("120002"), "coordinate jump detected (1893.0 m > 500.0 m)");
    }
    auto content = readFile(path);
    EXPECT_NE(content.find("rejected"), std::string::npos);
    EXPECT_NE(content.find("coordinate jump"), std::string::npos);
}

TEST_F(FileOutputTest, WritesErrorToFile)
{
    auto path = tmpFile("out.log");
    {
        FileOutput fo(path, 0, 0);
        fo.writeError("[12:00:05] Parse error: invalid checksum");
    }
    auto content = readFile(path);
    EXPECT_NE(content.find("invalid checksum"), std::string::npos);
}

TEST_F(FileOutputTest, ImplementsIOutputInterface)
{
    auto path = tmpFile("out.log");
    FileOutput fo(path, 0, 0);
    IOutput* iface = &fo;
    iface->writePoint(makePoint());
    iface->writeRejected(makePoint(), "test");
    iface->writeError("err");
    EXPECT_TRUE(fs::exists(path));
}

// ===========================================================================
// Rotation
// ===========================================================================

TEST_F(FileOutputTest, RotationCreatesNewFile)
{
    auto path = tmpFile("out.log");
    // maxSizeKb = 1 → rotate after 1 KB
    FileOutput fo(path, 1, 5);

    // Write enough data to trigger rotation (1 point ~ 120 bytes; 10 = ~1.2 KB)
    for (int i = 0; i < 10; ++i)
        fo.writePoint(makePoint());

    // After rotation, both out.log and out.log.1 should exist
    EXPECT_TRUE(fs::exists(path));
    EXPECT_TRUE(fs::exists(path + ".1"));
}

TEST_F(FileOutputTest, RotationShiftsOldFiles)
{
    auto path = tmpFile("rot.log");
    // Very small limit to force multiple rotations
    FileOutput fo(path, 1, 5);

    for (int i = 0; i < 30; ++i)
        fo.writePoint(makePoint());

    // At least two archived files should exist
    EXPECT_TRUE(fs::exists(path + ".1"));
    EXPECT_TRUE(fs::exists(path + ".2"));
}

TEST_F(FileOutputTest, RotationRespectsMaxFiles)
{
    auto path = tmpFile("max.log");
    // maxFiles = 2 → keep only .1 and .2
    FileOutput fo(path, 1, 2);

    for (int i = 0; i < 40; ++i)
        fo.writePoint(makePoint());

    EXPECT_FALSE(fs::exists(path + ".3"));
}

TEST_F(FileOutputTest, NoRotationWhenMaxSizeZero)
{
    auto path = tmpFile("nrot.log");
    FileOutput fo(path, 0, 5);  // no rotation

    for (int i = 0; i < 20; ++i)
        fo.writePoint(makePoint());

    EXPECT_FALSE(fs::exists(path + ".1"));
}

// ===========================================================================
// Invalid path
// ===========================================================================

TEST_F(FileOutputTest, InvalidPathThrows)
{
    EXPECT_THROW(
        FileOutput("/no/such/directory/out.log", 0, 0),
        std::runtime_error);
}
