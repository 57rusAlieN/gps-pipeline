#include <gtest/gtest.h>

#include "pipeline/NmeaRecordReader.h"
#include "pipeline/BinaryRecordReader.h"

#include <array>
#include <cstring>
#include <sstream>
#include <string>

// ===========================================================================
// Helpers shared with test_binary_parser.cpp — inline here for self-containment
// ===========================================================================

static std::string makeBinaryBlob(
    uint8_t  navValid  = 0,
    uint64_t datetime  = 1706356800ULL, // 2024-01-27 12:00:00 UTC
    int32_t  lat       = 55752057,
    int32_t  lon       = 37659463,
    int16_t  height    = 150,
    uint16_t speed     = 463,
    uint16_t course    = 450,
    uint8_t  hdop      = 8,
    uint8_t  satCount  = 10)
{
    std::array<uint8_t, 196> buf{};
    auto wr = [&](std::size_t off, auto v) {
        std::memcpy(buf.data() + off, &v, sizeof(v));
    };
    wr(8,  datetime);
    wr(16, navValid);
    wr(17, lat);
    wr(21, lon);
    wr(25, height);
    wr(27, speed);
    wr(29, course);
    wr(31, hdop);
    wr(33, satCount);
    return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
}

// ===========================================================================
// NmeaRecordReader tests
// ===========================================================================

namespace {

constexpr const char* kRmcMoving =
    "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70\n";
constexpr const char* kGgaMoving =
    "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E\n";
constexpr const char* kRmcVoid =
    "$GPRMC,120004,V,,,,,,,270124,,,N*56\n";
constexpr const char* kRmcBadCs =
    "$GPRMC,120005,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*00\n";

} // namespace

class NmeaReaderTest : public ::testing::Test {};

// ---------------------------------------------------------------------------
// hasNext / basic parse
// ---------------------------------------------------------------------------

TEST_F(NmeaReaderTest, EmptyStream_HasNextFalse)
{
    std::istringstream in("");
    NmeaRecordReader reader(in);
    EXPECT_FALSE(reader.hasNext());
}

TEST_F(NmeaReaderTest, SingleLine_HasNextTrue)
{
    std::istringstream in(kRmcMoving);
    NmeaRecordReader reader(in);
    EXPECT_TRUE(reader.hasNext());
}

TEST_F(NmeaReaderTest, ValidPair_YieldsPointRecord)
{
    std::string data = std::string(kRmcMoving) + kGgaMoving;
    std::istringstream in(data);
    NmeaRecordReader reader(in);

    // hasNext/readNext until we get a point
    ParsedRecord rec;
    bool gotPoint = false;
    while (reader.hasNext())
    {
        rec = reader.readNext();
        if (rec.point.has_value()) { gotPoint = true; break; }
    }
    ASSERT_TRUE(gotPoint);
    EXPECT_NEAR(rec.point->speed_kmh, 25.0 * 1.852, 1e-3);
}

TEST_F(NmeaReaderTest, ValidPair_TimeCorrect)
{
    std::string data = std::string(kRmcMoving) + kGgaMoving;
    std::istringstream in(data);
    NmeaRecordReader reader(in);

    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (rec.point.has_value())
        {
            EXPECT_EQ(rec.point->time, "120000");
            break;
        }
    }
}

TEST_F(NmeaReaderTest, VoidRmc_YieldsErrorRecord)
{
    std::istringstream in(kRmcVoid);
    NmeaRecordReader reader(in);

    bool gotError = false;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (!rec.point.has_value() && !rec.error.empty())
        {
            gotError = true;
            EXPECT_NE(rec.error.find("No valid GPS fix"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(gotError);
}

TEST_F(NmeaReaderTest, BadChecksum_YieldsErrorRecord)
{
    std::istringstream in(kRmcBadCs);
    NmeaRecordReader reader(in);

    bool gotError = false;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (!rec.point.has_value() && !rec.error.empty())
        {
            gotError = true;
            EXPECT_NE(rec.error.find("invalid checksum"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(gotError);
}

TEST_F(NmeaReaderTest, EmptyAndCommentLines_Skipped)
{
    std::istringstream in("# comment\n\n# another\n");
    NmeaRecordReader reader(in);
    // Nothing to read — all lines are comments/empty
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        EXPECT_FALSE(rec.point.has_value());
        EXPECT_TRUE(rec.error.empty());  // silent skip, no error
    }
}

TEST_F(NmeaReaderTest, MultipleValidPairs_AllDelivered)
{
    constexpr const char* kRmc2 =
        "$GPRMC,120001,A,5545.1235,N,03739.5679,E,001.2,045.0,270124,,,A*75\n";
    constexpr const char* kGga2 =
        "$GPGGA,120001,5545.1235,N,03739.5679,E,1,10,0.8,150.0,M,14.0,M,,*4F\n";

    std::string data =
        std::string(kRmcMoving) + kGgaMoving +
        std::string(kRmc2)     + kGga2;
    std::istringstream in(data);
    NmeaRecordReader reader(in);

    int points = 0;
    while (reader.hasNext())
    {
        auto rec = reader.readNext();
        if (rec.point.has_value()) ++points;
    }
    EXPECT_EQ(points, 2);
}

// ===========================================================================
// BinaryRecordReader tests
// ===========================================================================

class BinaryReaderTest : public ::testing::Test {};

TEST_F(BinaryReaderTest, EmptyStream_HasNextFalse)
{
    std::istringstream in("");
    BinaryRecordReader reader(in);
    EXPECT_FALSE(reader.hasNext());
}

TEST_F(BinaryReaderTest, ValidRecord_HasNextTrue)
{
    std::istringstream in(makeBinaryBlob());
    BinaryRecordReader reader(in);
    EXPECT_TRUE(reader.hasNext());
}

TEST_F(BinaryReaderTest, ValidRecord_YieldsPoint)
{
    std::istringstream in(makeBinaryBlob());
    BinaryRecordReader reader(in);

    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    ASSERT_TRUE(rec.point.has_value());
    EXPECT_NEAR(rec.point->lat,       55.752057, 1e-5);
    EXPECT_NEAR(rec.point->speed_kmh, 46.3,      1e-3);
}

TEST_F(BinaryReaderTest, InvalidNav_YieldsErrorRecord)
{
    std::istringstream in(makeBinaryBlob(1));  // RCVR_ERR
    BinaryRecordReader reader(in);

    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    EXPECT_FALSE(rec.point.has_value());
    EXPECT_FALSE(rec.error.empty());
    EXPECT_NE(rec.error.find("error"), std::string::npos);
}

TEST_F(BinaryReaderTest, FirstStartNav_YieldsErrorRecord)
{
    std::istringstream in(makeBinaryBlob(0xFF));
    BinaryRecordReader reader(in);

    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    EXPECT_FALSE(rec.point.has_value());
    EXPECT_FALSE(rec.error.empty());
}

TEST_F(BinaryReaderTest, MultipleRecords_AllDelivered)
{
    std::string blob = makeBinaryBlob() + makeBinaryBlob() + makeBinaryBlob();
    std::istringstream in(blob);
    BinaryRecordReader reader(in);

    int count = 0;
    while (reader.hasNext())
    {
        reader.readNext();
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST_F(BinaryReaderTest, PartialFinalRecord_Ignored)
{
    std::string blob = makeBinaryBlob() + std::string(50, '\x00');
    std::istringstream in(blob);
    BinaryRecordReader reader(in);

    int count = 0;
    while (reader.hasNext())
    {
        reader.readNext();
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(BinaryReaderTest, ErrorMessage_ContainsTime)
{
    // datetime = 1706356800 = 2024-01-27 12:00:00 UTC
    std::istringstream in(makeBinaryBlob(1, 1706356800ULL));
    BinaryRecordReader reader(in);

    ASSERT_TRUE(reader.hasNext());
    auto rec = reader.readNext();
    EXPECT_NE(rec.error.find("12:00:00"), std::string::npos);
}

TEST_F(BinaryReaderTest, AfterLastRecord_HasNextFalse)
{
    std::istringstream in(makeBinaryBlob());
    BinaryRecordReader reader(in);

    ASSERT_TRUE(reader.hasNext());
    reader.readNext();
    EXPECT_FALSE(reader.hasNext());
}
