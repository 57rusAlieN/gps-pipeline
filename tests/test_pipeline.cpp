#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "pipeline/Pipeline.h"
#include "pipeline/NmeaRecordReader.h"
#include "filter/FilterChain.h"
#include "filter/SatelliteFilter.h"
#include "filter/SpeedFilter.h"
#include "filter/CoordinateJumpFilter.h"
#include "filter/StopFilter.h"
#include "output/IOutput.h"

// ---------------------------------------------------------------------------
// MockOutput — captures every call from Pipeline for verification
// ---------------------------------------------------------------------------
namespace {

struct MockEntry
{
    enum Type { Point, Rejected, Error } type;
    GpsPoint    point{};
    std::string message;  // reason (Rejected) or error text (Error)
};

struct MockOutput final : public IOutput
{
    std::vector<MockEntry> entries;

    void writePoint(const GpsPoint& p) override
    {
        entries.push_back({MockEntry::Point, p, ""});
    }
    void writeRejected(const GpsPoint& p, const std::string& r) override
    {
        entries.push_back({MockEntry::Rejected, p, r});
    }
    void writeError(const std::string& msg) override
    {
        MockEntry e; e.type = MockEntry::Error; e.message = msg;
        entries.push_back(e);
    }
};

// ---------------------------------------------------------------------------
// Sentences from data/sample.nmea (checksums verified)
// ---------------------------------------------------------------------------

constexpr const char* kRmc0 = "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70";
constexpr const char* kGga0 = "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E";

constexpr const char* kRmc1 = "$GPRMC,120001,A,5545.1235,N,03739.5679,E,001.2,045.0,270124,,,A*75";
constexpr const char* kGga1 = "$GPGGA,120001,5545.1235,N,03739.5679,E,1,10,0.8,150.0,M,14.0,M,,*4F";

constexpr const char* kRmc2 = "$GPRMC,120002,A,5546.0000,N,03740.5000,E,025.0,045.0,270124,,,A*72";
constexpr const char* kGga2 = "$GPGGA,120002,5546.0000,N,03740.5000,E,1,10,0.8,150.0,M,14.0,M,,*4C";

constexpr const char* kRmc3 = "$GPRMC,120003,A,5545.1236,N,03739.5680,E,025.0,045.0,270124,,,A*76";
constexpr const char* kGga3 = "$GPGGA,120003,5545.1236,N,03739.5680,E,1,03,2.5,150.0,M,14.0,M,,*45";

constexpr const char* kRmcVoid  = "$GPRMC,120004,V,,,,,,,270124,,,N*56";
constexpr const char* kRmcBadCs = "$GPRMC,120005,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*00";

// ---------------------------------------------------------------------------
// Test fixture
//   - FilterChain and MockOutput persist across feed() calls so filter state
//     (CoordinateJumpFilter, StopFilter) accumulates correctly.
//   - Each feed() creates a fresh NmeaRecordReader + Pipeline so the NMEA
//     parser resets between test cases.
// ---------------------------------------------------------------------------

class PipelineTest : public ::testing::Test
{
protected:
    FilterChain filters;
    MockOutput  output;

    PipelineTest()
    {
        filters.add(std::make_unique<SatelliteFilter>(4));
        filters.add(std::make_unique<SpeedFilter>(200.0));
        filters.add(std::make_unique<CoordinateJumpFilter>(500.0));
        filters.add(std::make_unique<StopFilter>(3.0));
    }

    // Feed lines through a fresh NmeaRecordReader into persistent filters+output.
    void feed(std::initializer_list<const char*> lines)
    {
        std::string data;
        for (const char* l : lines) { data += l; data += '\n'; }
        std::istringstream in(data);
        NmeaRecordReader reader(in);
        Pipeline pipe(reader, filters, output);
        pipe.run();
    }
};

} // namespace

// ===========================================================================
// Unit tests
// ===========================================================================

TEST_F(PipelineTest, SkipsEmptyLine)
{
    feed({""});
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, SkipsCommentLine)
{
    feed({"# comment"});
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, SkipsNonNmeaLine)
{
    feed({"some random text"});
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, InvalidChecksumReportsError)
{
    feed({kRmcBadCs});
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Error);
    EXPECT_NE(output.entries[0].message.find("invalid checksum"), std::string::npos);
    EXPECT_NE(output.entries[0].message.find("[12:00:05]"), std::string::npos);
}

TEST_F(PipelineTest, VoidRmcReportsNoFix)
{
    feed({kRmcVoid});
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Error);
    EXPECT_NE(output.entries[0].message.find("No valid GPS fix"), std::string::npos);
    EXPECT_NE(output.entries[0].message.find("[12:00:04]"), std::string::npos);
}

TEST_F(PipelineTest, SingleRmcProducesNoOutput)
{
    // RMC only → waiting for GGA pair → silent
    feed({kRmc0});
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, ValidPairEmitsPoint)
{
    feed({kRmc0, kGga0});
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Point);
    EXPECT_EQ(output.entries[0].point.time, "120000");
    EXPECT_FALSE(output.entries[0].point.stopped);
}

TEST_F(PipelineTest, SlowPairEmitsStoppedPoint)
{
    // Both pairs in one feed so NmeaParser state and FilterChain state are shared.
    feed({kRmc0, kGga0, kRmc1, kGga1});
    ASSERT_EQ(output.entries.size(), 2u);
    EXPECT_EQ(output.entries[1].type, MockEntry::Point);
    EXPECT_TRUE(output.entries[1].point.stopped);
}

TEST_F(PipelineTest, CoordinateJumpRejected)
{
    feed({kRmc0, kGga0, kRmc1, kGga1, kRmc2, kGga2});
    ASSERT_GE(output.entries.size(), 3u);
    EXPECT_EQ(output.entries[2].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[2].message.find("jump"), std::string::npos);
}

TEST_F(PipelineTest, InsufficientSatellitesRejected)
{
    feed({kRmc0, kGga0, kRmc3, kGga3});
    ASSERT_GE(output.entries.size(), 2u);
    const auto& last = output.entries.back();
    EXPECT_EQ(last.type, MockEntry::Rejected);
    EXPECT_NE(last.message.find("satellite"), std::string::npos);
}

// ===========================================================================
// E2E — full data/sample.nmea sequence
// ===========================================================================

TEST_F(PipelineTest, FullSampleNmeaSequence)
{
    feed({
        "# Валидные данные — движение",
        kRmc0, kGga0,
        "",
        "# Валидные данные — остановка",
        kRmc1, kGga1,
        "",
        "# Скачок координат",
        kRmc2, kGga2,
        "",
        "# Мало спутников",
        kRmc3, kGga3,
        "",
        "# Невалидный фикс (status=V)",
        kRmcVoid,
        "",
        "# Неверная контрольная сумма",
        kRmcBadCs,
    });

    ASSERT_EQ(output.entries.size(), 6u);

    EXPECT_EQ(output.entries[0].type, MockEntry::Point);
    EXPECT_EQ(output.entries[0].point.time, "120000");
    EXPECT_FALSE(output.entries[0].point.stopped);
    EXPECT_NEAR(output.entries[0].point.speed_kmh, 46.3, 0.1);

    EXPECT_EQ(output.entries[1].type, MockEntry::Point);
    EXPECT_EQ(output.entries[1].point.time, "120001");
    EXPECT_TRUE(output.entries[1].point.stopped);

    EXPECT_EQ(output.entries[2].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[2].message.find("jump"), std::string::npos);

    EXPECT_EQ(output.entries[3].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[3].message.find("satellite"), std::string::npos);

    EXPECT_EQ(output.entries[4].type, MockEntry::Error);
    EXPECT_NE(output.entries[4].message.find("No valid GPS fix"), std::string::npos);

    EXPECT_EQ(output.entries[5].type, MockEntry::Error);
    EXPECT_NE(output.entries[5].message.find("invalid checksum"), std::string::npos);
}

