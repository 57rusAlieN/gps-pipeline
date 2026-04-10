#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pipeline/Pipeline.h"
#include "parser/NmeaParser.h"
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
    GpsPoint point{};
    std::string message;   // reason (Rejected) or error text (Error)
};

struct MockOutput final : public IOutput
{
    std::vector<MockEntry> entries;

    void writePoint(const GpsPoint& p) override
    {
        entries.push_back({MockEntry::Point, p, ""});
    }
    void writeRejected(const GpsPoint& p, const std::string& reason) override
    {
        entries.push_back({MockEntry::Rejected, p, reason});
    }
    void writeError(const std::string& msg) override
    {
        MockEntry e;
        e.type    = MockEntry::Error;
        e.message = msg;
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
// Test fixture — wires NmeaParser + FilterChain + MockOutput → Pipeline
// ---------------------------------------------------------------------------

class PipelineTest : public ::testing::Test
{
protected:
    NmeaParser  parser;
    FilterChain filters;
    MockOutput  output;
    Pipeline    pipeline{parser, filters, output};

    PipelineTest()
    {
        // Order: validation first, correction last
        filters.add(std::make_unique<SatelliteFilter>(4));
        filters.add(std::make_unique<SpeedFilter>(200.0));
        filters.add(std::make_unique<CoordinateJumpFilter>(500.0));
        filters.add(std::make_unique<StopFilter>(3.0));   // < 3 km/h → stopped
    }
};

} // namespace

// ===========================================================================
// Unit tests — individual Pipeline::processLine behaviours
// ===========================================================================

TEST_F(PipelineTest, SkipsEmptyLine)
{
    pipeline.processLine("");
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, SkipsCommentLine)
{
    pipeline.processLine("# Валидные данные — движение");
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, SkipsNonNmeaLine)
{
    pipeline.processLine("some random text");
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, InvalidChecksumReportsError)
{
    pipeline.processLine(kRmcBadCs);
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Error);
    EXPECT_NE(output.entries[0].message.find("invalid checksum"), std::string::npos);
    EXPECT_NE(output.entries[0].message.find("[12:00:05]"), std::string::npos);
}

TEST_F(PipelineTest, VoidRmcReportsNoFix)
{
    pipeline.processLine(kRmcVoid);
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Error);
    EXPECT_NE(output.entries[0].message.find("No valid GPS fix"), std::string::npos);
    EXPECT_NE(output.entries[0].message.find("[12:00:04]"), std::string::npos);
}

TEST_F(PipelineTest, SingleRmcProducesNoOutput)
{
    // RMC stored, waiting for GGA pair → no output yet
    pipeline.processLine(kRmc0);
    EXPECT_TRUE(output.entries.empty());
}

TEST_F(PipelineTest, ValidPairEmitsPoint)
{
    pipeline.processLine(kRmc0);
    pipeline.processLine(kGga0);
    ASSERT_EQ(output.entries.size(), 1u);
    EXPECT_EQ(output.entries[0].type, MockEntry::Point);
    EXPECT_EQ(output.entries[0].point.time, "120000");
    EXPECT_FALSE(output.entries[0].point.stopped);
}

TEST_F(PipelineTest, SlowPairEmitsStoppedPoint)
{
    // Need a first point to establish position for JumpFilter
    pipeline.processLine(kRmc0);
    pipeline.processLine(kGga0);

    pipeline.processLine(kRmc1);
    pipeline.processLine(kGga1);
    ASSERT_EQ(output.entries.size(), 2u);
    EXPECT_EQ(output.entries[1].type, MockEntry::Point);
    EXPECT_TRUE(output.entries[1].point.stopped);
}

TEST_F(PipelineTest, CoordinateJumpRejected)
{
    // Establish base
    pipeline.processLine(kRmc0);
    pipeline.processLine(kGga0);
    // Establish second position (updates prev in JumpFilter)
    pipeline.processLine(kRmc1);
    pipeline.processLine(kGga1);
    // Feed the jump pair
    pipeline.processLine(kRmc2);
    pipeline.processLine(kGga2);
    ASSERT_GE(output.entries.size(), 3u);
    EXPECT_EQ(output.entries[2].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[2].message.find("jump"), std::string::npos);
}

TEST_F(PipelineTest, InsufficientSatellitesRejected)
{
    // Establish base
    pipeline.processLine(kRmc0);
    pipeline.processLine(kGga0);

    pipeline.processLine(kRmc3);
    pipeline.processLine(kGga3);
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
    const std::vector<std::string> lines = {
        "# Валидные данные — движение",
        kRmc0, kGga0,
        "",
        "# Валидные данные — остановка (низкая скорость)",
        kRmc1, kGga1,
        "",
        "# Скачок координат (ошибка GPS)",
        kRmc2, kGga2,
        "",
        "# Мало спутников",
        kRmc3, kGga3,
        "",
        "# Невалидный фикс (status=V)",
        kRmcVoid,
        "",
        "# Неверная контрольная сумма (должна быть *75)",
        kRmcBadCs,
    };

    for (const auto& line : lines)
        pipeline.processLine(line);

    // Expected 6 output entries
    ASSERT_EQ(output.entries.size(), 6u);

    // 1. Normal movement
    EXPECT_EQ(output.entries[0].type, MockEntry::Point);
    EXPECT_EQ(output.entries[0].point.time, "120000");
    EXPECT_FALSE(output.entries[0].point.stopped);
    EXPECT_NEAR(output.entries[0].point.speed_kmh, 46.3, 0.1);

    // 2. Slow speed → stopped
    EXPECT_EQ(output.entries[1].type, MockEntry::Point);
    EXPECT_EQ(output.entries[1].point.time, "120001");
    EXPECT_TRUE(output.entries[1].point.stopped);

    // 3. Coordinate jump → rejected
    EXPECT_EQ(output.entries[2].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[2].message.find("jump"), std::string::npos);

    // 4. Insufficient satellites → rejected
    EXPECT_EQ(output.entries[3].type, MockEntry::Rejected);
    EXPECT_NE(output.entries[3].message.find("satellite"), std::string::npos);

    // 5. Void RMC → error "No valid GPS fix"
    EXPECT_EQ(output.entries[4].type, MockEntry::Error);
    EXPECT_NE(output.entries[4].message.find("No valid GPS fix"), std::string::npos);

    // 6. Bad checksum → error "invalid checksum"
    EXPECT_EQ(output.entries[5].type, MockEntry::Error);
    EXPECT_NE(output.entries[5].message.find("invalid checksum"), std::string::npos);
}
