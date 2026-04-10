#include <gtest/gtest.h>

#include "parser/NmeaParser.h"

// ---------------------------------------------------------------------------
// Test sentences from data/sample.nmea — checksums verified with XOR
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kRmcMoving = "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70";
constexpr const char* kGgaMoving = "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E";

constexpr const char* kRmcSlow   = "$GPRMC,120001,A,5545.1235,N,03739.5679,E,001.2,045.0,270124,,,A*75";
constexpr const char* kGgaSlow   = "$GPGGA,120001,5545.1235,N,03739.5679,E,1,10,0.8,150.0,M,14.0,M,,*4F";

constexpr const char* kRmcVoid   = "$GPRMC,120004,V,,,,,,,270124,,,N*56";
constexpr const char* kRmcBadCs  = "$GPRMC,120005,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*00";

class NmeaParserTest : public ::testing::Test
{
protected:
    NmeaParser parser;
};

} // namespace

// ---------------------------------------------------------------------------
// Single-sentence — parser waits for the pair
// ---------------------------------------------------------------------------

TEST_F(NmeaParserTest, ParseRmcAloneReturnsNullopt)
{
    EXPECT_FALSE(parser.parse(kRmcMoving).has_value());
}

TEST_F(NmeaParserTest, ParseGgaAloneReturnsNullopt)
{
    EXPECT_FALSE(parser.parse(kGgaMoving).has_value());
}

// ---------------------------------------------------------------------------
// Complete RMC+GGA pairs → GpsPoint emitted
// ---------------------------------------------------------------------------

TEST_F(NmeaParserTest, ParseRmcThenGgaEmitsPoint)
{
    ASSERT_FALSE(parser.parse(kRmcMoving).has_value());
    const auto result = parser.parse(kGgaMoving);
    ASSERT_TRUE(result.has_value());
}

TEST_F(NmeaParserTest, ParseGgaThenRmcEmitsPoint)
{
    ASSERT_FALSE(parser.parse(kGgaMoving).has_value());
    const auto result = parser.parse(kRmcMoving);
    ASSERT_TRUE(result.has_value());
}

// ---------------------------------------------------------------------------
// GpsPoint field values
// ---------------------------------------------------------------------------

TEST_F(NmeaParserTest, PointHasCorrectCoordinates)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    // 5545.1234 N  →  55 + 45.1234 / 60 = 55.752056(6)
    EXPECT_NEAR(p->lat, 55.752056, 1e-5);
    // 03739.5678 E →  37 + 39.5678 / 60 = 37.659463(3)
    EXPECT_NEAR(p->lon, 37.659463, 1e-5);
}

TEST_F(NmeaParserTest, PointHasCorrectSpeed)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    // 25.0 knots × 1.852 = 46.3 km/h
    EXPECT_NEAR(p->speed_kmh, 46.3, 1e-3);
}

TEST_F(NmeaParserTest, PointHasCorrectCourse)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->course, 45.0, 1e-6);
}

TEST_F(NmeaParserTest, PointHasCorrectAltitudeAndSatellites)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    EXPECT_NEAR(p->altitude,   150.0, 1e-6);
    EXPECT_EQ  (p->satellites, 10);
    EXPECT_NEAR(p->hdop,       0.8,   1e-6);
}

TEST_F(NmeaParserTest, PointHasCorrectTime)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->time, "120000");
}

TEST_F(NmeaParserTest, PointStoppedFlagDefaultsFalse)
{
    parser.parse(kRmcMoving);
    const auto p = parser.parse(kGgaMoving);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->stopped);  // StopFilter sets this, not the parser
}

// ---------------------------------------------------------------------------
// Error / invalid-input cases
// ---------------------------------------------------------------------------

TEST_F(NmeaParserTest, RmcVoidStatusReturnsNullopt)
{
    EXPECT_FALSE(parser.parse(kRmcVoid).has_value());
}

TEST_F(NmeaParserTest, InvalidChecksumReturnsNullopt)
{
    EXPECT_FALSE(parser.parse(kRmcBadCs).has_value());
}

TEST_F(NmeaParserTest, UnknownSentenceTypeReturnsNullopt)
{
    // $GPGSV is valid NMEA but not handled — silently ignored
    EXPECT_FALSE(parser.parse("$GPGSV,2,1,08,01,40,083,46*4C").has_value());
}

TEST_F(NmeaParserTest, EmptyLineReturnsNullopt)
{
    EXPECT_FALSE(parser.parse("").has_value());
}

TEST_F(NmeaParserTest, CommentLineReturnsNullopt)
{
    // Lines starting with '#' (as in sample.nmea) must be silently skipped
    EXPECT_FALSE(parser.parse("# Валидные данные — движение").has_value());
}

// ---------------------------------------------------------------------------
// State-machine / pairing logic
// ---------------------------------------------------------------------------

TEST_F(NmeaParserTest, DifferentTimesDoNotEmit)
{
    // RMC time=120000, GGA time=120001 → no emit (timestamp mismatch)
    parser.parse(kRmcMoving);
    EXPECT_FALSE(parser.parse(kGgaSlow).has_value());
}

TEST_F(NmeaParserTest, SlowSpeedPointConvertsKnotsCorrectly)
{
    parser.parse(kRmcSlow);
    const auto p = parser.parse(kGgaSlow);
    ASSERT_TRUE(p.has_value());
    // 1.2 knots × 1.852 = 2.2224 km/h
    EXPECT_NEAR(p->speed_kmh, 1.2 * 1.852, 1e-6);
}

TEST_F(NmeaParserTest, LowSatellitePointParsedNormally)
{
    // 3 satellites — filtering is the SatelliteFilter's job, not the parser's
    const char* rmcFew = "$GPRMC,120003,A,5545.1236,N,03739.5680,E,025.0,045.0,270124,,,A*76";
    const char* ggaFew = "$GPGGA,120003,5545.1236,N,03739.5680,E,1,03,2.5,150.0,M,14.0,M,,*45";
    parser.parse(rmcFew);
    const auto p = parser.parse(ggaFew);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->satellites, 3);
    EXPECT_NEAR(p->hdop, 2.5, 1e-6);
}

TEST_F(NmeaParserTest, StateResetsAfterEmit)
{
    // First pair
    parser.parse(kRmcMoving);
    ASSERT_TRUE(parser.parse(kGgaMoving).has_value());

    // Second pair — state must be clean so no stale data bleeds through
    parser.parse(kRmcSlow);
    const auto second = parser.parse(kGgaSlow);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->time, "120001");
    EXPECT_NEAR(second->speed_kmh, 1.2 * 1.852, 1e-6);
}
