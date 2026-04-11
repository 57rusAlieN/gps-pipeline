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

// Two GSV sentences covering 5 satellites (total_in_view = 5)
constexpr const char* kGsv1 =
    "$GPGSV,2,1,05,07,84,069,43,17,49,161,43,15,47,270,43,18,36,314,44*79";
constexpr const char* kGsv2 =
    "$GPGSV,2,2,05,08,20,296,37*4F";

} // namespace

class NmeaParserTest : public ::testing::Test
{
protected:
    NmeaParser parser;
};

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

// ===========================================================================
// GPGSV — satellite-in-view data
// ===========================================================================

TEST_F(NmeaParserTest, GsvAloneReturnsNullopt)
{
    EXPECT_FALSE(parser.parse(kGsv1).has_value());
    EXPECT_FALSE(parser.parse(kGsv2).has_value());
}

TEST_F(NmeaParserTest, GsvBeforePairAttachesSatelliteData)
{
    // Feed two GSV sentences, then the RMC+GGA pair
    parser.parse(kGsv1);
    parser.parse(kGsv2);
    parser.parse(kRmcMoving);
    const auto pt = parser.parse(kGgaMoving);

    ASSERT_TRUE(pt.has_value());
    EXPECT_EQ(pt->satellites_in_view.size(), 5u);
}

TEST_F(NmeaParserTest, GsvFirstSatelliteFieldsCorrect)
{
    parser.parse(kGsv1);
    parser.parse(kGsv2);
    parser.parse(kRmcMoving);
    const auto pt = parser.parse(kGgaMoving);

    ASSERT_TRUE(pt.has_value());
    ASSERT_GE(pt->satellites_in_view.size(), 1u);

    const auto& sat = pt->satellites_in_view[0];
    EXPECT_EQ(sat.prn,       7);
    EXPECT_EQ(sat.elevation, 84);
    EXPECT_EQ(sat.azimuth,   69);
    EXPECT_EQ(sat.snr,       43);
}

TEST_F(NmeaParserTest, GsvLastSatelliteFromSecondMessage)
{
    parser.parse(kGsv1);
    parser.parse(kGsv2);
    parser.parse(kRmcMoving);
    const auto pt = parser.parse(kGgaMoving);

    ASSERT_TRUE(pt.has_value());
    ASSERT_EQ(pt->satellites_in_view.size(), 5u);

    // Last sat (PRN 8) is from the second GSV sentence
    const auto& last = pt->satellites_in_view[4];
    EXPECT_EQ(last.prn,       8);
    EXPECT_EQ(last.elevation, 20);
    EXPECT_EQ(last.azimuth,   296);
    EXPECT_EQ(last.snr,       37);
}

TEST_F(NmeaParserTest, NoGsvLeavesEmptySatelliteList)
{
    parser.parse(kRmcMoving);
    const auto pt = parser.parse(kGgaMoving);

    ASSERT_TRUE(pt.has_value());
    EXPECT_TRUE(pt->satellites_in_view.empty());
}

TEST_F(NmeaParserTest, GsvDataClearedAfterEmit)
{
    // First point WITH GSV data
    parser.parse(kGsv1);
    parser.parse(kGsv2);
    parser.parse(kRmcMoving);
    parser.parse(kGgaMoving);

    // Second point WITHOUT GSV data — should have empty satellites_in_view
    parser.parse(kRmcSlow);
    const auto pt2 = parser.parse(kGgaSlow);
    ASSERT_TRUE(pt2.has_value());
    EXPECT_TRUE(pt2->satellites_in_view.empty());
}

