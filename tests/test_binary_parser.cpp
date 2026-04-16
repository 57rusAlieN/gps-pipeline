#include <gtest/gtest.h>

#include "parser/GnssBinaryParser.h"

#include <array>
#include <cstring>
#include <sstream>
#include <string>

// ===========================================================================
// RecordBuilder — builds a valid 196-byte binary record at known offsets.
// Offsets are taken directly from docs/BINARY_FORMAT.md.
// ===========================================================================
struct RecordBuilder
{
    std::array<uint8_t, 196> buf{};

    // Helper: write value T at byte offset (little-endian / memcpy)
    template<typename T>
    RecordBuilder& at(std::size_t offset, T value)
    {
        static_assert(sizeof(T) <= 8);
        std::memcpy(buf.data() + offset, &value, sizeof(T));
        return *this;
    }

    RecordBuilder& setTimestamp (uint64_t v) { return at<uint64_t>(0,  v); }
    RecordBuilder& setDatetime  (uint64_t v) { return at<uint64_t>(8,  v); }
    RecordBuilder& setNavValid  (uint8_t  v) { return at<uint8_t> (16, v); }
    RecordBuilder& setLatitude  (int32_t  v) { return at<int32_t> (17, v); }
    RecordBuilder& setLongitude (int32_t  v) { return at<int32_t> (21, v); }
    RecordBuilder& setHeight    (int16_t  v) { return at<int16_t> (25, v); }
    RecordBuilder& setSpeed     (uint16_t v) { return at<uint16_t>(27, v); }
    RecordBuilder& setCourse    (uint16_t v) { return at<uint16_t>(29, v); }
    RecordBuilder& setHDOP      (uint8_t  v) { return at<uint8_t> (31, v); }
    RecordBuilder& setPDOP      (uint8_t  v) { return at<uint8_t> (32, v); }
    RecordBuilder& setSatCount  (uint8_t  v) { return at<uint8_t> (33, v); }

    // overallArraySnr[idx] at offset 100+idx
    // bit7 = isUse, bits0-6 = snr
    RecordBuilder& setSnr(int idx, uint8_t snr, bool inUse)
    {
        uint8_t byte = (snr & 0x7Fu) | (inUse ? 0x80u : 0u);
        buf[100 + idx] = byte;
        return *this;
    }

    // Convenience: build a default valid record
    static RecordBuilder valid()
    {
        return RecordBuilder{}
            // 2024-01-27 12:00:00 UTC = 1706356800
            .setDatetime (1706356800ULL)
            .setNavValid (0)              // STATUS_VALID
            .setLatitude (55752057)       // 55.752057°N
            .setLongitude(37659463)       // 37.659463°E
            .setHeight   (150)            // 150 m
            .setSpeed    (463)            // 46.3 km/h
            .setCourse   (450)            // 45.0°
            .setHDOP     (8)              // 0.8
            .setSatCount (10);
    }

    const uint8_t* data() const { return buf.data(); }
    std::size_t    size() const { return buf.size(); }

    // Build a std::string blob usable as istream input
    std::string asString() const
    {
        return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
    }
};

// ===========================================================================
// GnssBinaryParser unit tests
// ===========================================================================
class GnssBinaryParserTest : public ::testing::Test
{
protected:
    GnssBinaryParser parser;
};

// ---------------------------------------------------------------------------
// Positive: valid record → GpsPoint with correct values
// ---------------------------------------------------------------------------

TEST_F(GnssBinaryParserTest, ValidRecord_ReturnsGpsPoint)
{
    auto rb = RecordBuilder::valid();
    EXPECT_TRUE(parser.parseRecord(rb.data(), rb.size()).has_value());
}

TEST_F(GnssBinaryParserTest, CorrectLatitude)
{
    auto rb = RecordBuilder::valid();
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->lat, 55.752057, 1e-5);
}

TEST_F(GnssBinaryParserTest, CorrectLongitude)
{
    auto rb = RecordBuilder::valid();
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->lon, 37.659463, 1e-5);
}

TEST_F(GnssBinaryParserTest, CorrectSpeed)
{
    auto rb = RecordBuilder::valid().setSpeed(463);   // 46.3 km/h
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->speed_kmh, 46.3, 1e-4);
}

TEST_F(GnssBinaryParserTest, ZeroSpeed)
{
    auto rb = RecordBuilder::valid().setSpeed(0);
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->speed_kmh, 0.0, 1e-6);
}

TEST_F(GnssBinaryParserTest, CorrectCourse)
{
    auto rb = RecordBuilder::valid().setCourse(3599);  // 359.9°
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->course, 359.9, 1e-4);
}

TEST_F(GnssBinaryParserTest, CorrectAltitude)
{
    auto rb = RecordBuilder::valid().setHeight(static_cast<int16_t>(-50)); // -50 m
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->altitude, -50.0, 1e-6);
}

TEST_F(GnssBinaryParserTest, CorrectHDOP)
{
    auto rb = RecordBuilder::valid().setHDOP(8);   // 0.8
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->hdop, 0.8, 1e-4);
}

TEST_F(GnssBinaryParserTest, CorrectSatellites)
{
    auto rb = RecordBuilder::valid().setSatCount(12);
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_EQ(pt->satellites, 12);
}

TEST_F(GnssBinaryParserTest, NegativeLatitude)
{
    auto rb = RecordBuilder::valid().setLatitude(-33868020); // -33.868020 (Sydney)
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_NEAR(pt->lat, -33.868020, 1e-5);
}

TEST_F(GnssBinaryParserTest, TimeFormattedFromUnixEpoch)
{
    // 1706356800 = 2024-01-27 12:00:00 UTC
    auto rb = RecordBuilder::valid().setDatetime(1706356800ULL);
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_EQ(pt->time, "120000");
}

TEST_F(GnssBinaryParserTest, StoppedFlagDefaultsFalse)
{
    auto rb = RecordBuilder::valid();
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_FALSE(pt->stopped);  // StopFilter sets this; parser does not
}

// ---------------------------------------------------------------------------
// navValid != STATUS_VALID → nullopt
// ---------------------------------------------------------------------------

TEST_F(GnssBinaryParserTest, ReceiverError_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid().setNavValid(1);  // RCVR_ERR
    EXPECT_FALSE(parser.parseRecord(rb.data(), rb.size()).has_value());
}

TEST_F(GnssBinaryParserTest, LBS_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid().setNavValid(2);  // LBS
    EXPECT_FALSE(parser.parseRecord(rb.data(), rb.size()).has_value());
}

TEST_F(GnssBinaryParserTest, ExternalLocation_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid().setNavValid(3);  // EL
    EXPECT_FALSE(parser.parseRecord(rb.data(), rb.size()).has_value());
}

TEST_F(GnssBinaryParserTest, FirstStart_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid().setNavValid(0xFF);  // FIRST_START
    EXPECT_FALSE(parser.parseRecord(rb.data(), rb.size()).has_value());
}

TEST_F(GnssBinaryParserTest, LastNavStatus_ReflectsLastRecord)
{
    auto rb = RecordBuilder::valid().setNavValid(1);
    parser.parseRecord(rb.data(), rb.size());
    EXPECT_EQ(parser.lastNavStatus(), 1u);
}

TEST_F(GnssBinaryParserTest, LastNavStatus_ValidRecord)
{
    auto rb = RecordBuilder::valid().setNavValid(0);
    parser.parseRecord(rb.data(), rb.size());
    EXPECT_EQ(parser.lastNavStatus(), 0u);
}

// ---------------------------------------------------------------------------
// Wrong size → nullopt
// ---------------------------------------------------------------------------

TEST_F(GnssBinaryParserTest, WrongSize_TooSmall_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid();
    EXPECT_FALSE(parser.parseRecord(rb.data(), 100).has_value());
}

TEST_F(GnssBinaryParserTest, WrongSize_TooBig_ReturnsNullopt)
{
    auto rb = RecordBuilder::valid();
    EXPECT_FALSE(parser.parseRecord(rb.data(), 200).has_value());
}

// ---------------------------------------------------------------------------
// satellites_in_view from overallArraySnr
// ---------------------------------------------------------------------------

TEST_F(GnssBinaryParserTest, SatellitesInView_Populated)
{
    auto rb = RecordBuilder::valid()
                  .setSnr(0,  43, true)   // GPS PRN 1, used
                  .setSnr(1,  40, true)   // GPS PRN 2, used
                  .setSnr(64, 35, false);  // GLONASS slot 1, visible only
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());
    EXPECT_GE(pt->satellites_in_view.size(), 3u);
}

TEST_F(GnssBinaryParserTest, SatellitesInView_SnrAndFlag)
{
    auto rb = RecordBuilder::valid()
                  .setSnr(0, 43, true)   // GPS PRN 1, used, snr=43
                  .setSnr(1,  0, false); // GPS PRN 2, not used, snr=0 — should be skipped
    auto pt = parser.parseRecord(rb.data(), rb.size());
    ASSERT_TRUE(pt.has_value());

    // Only PRN 1 (snr > 0)
    bool foundPrn1 = false;
    for (const auto& sv : pt->satellites_in_view)
    {
        if (sv.prn == 1)
        {
            EXPECT_EQ(sv.snr, 43);
            EXPECT_TRUE(sv.snr > 0);
            foundPrn1 = true;
        }
        EXPECT_GT(sv.snr, 0);  // zero-SNR sats should not appear
    }
    EXPECT_TRUE(foundPrn1);
}
