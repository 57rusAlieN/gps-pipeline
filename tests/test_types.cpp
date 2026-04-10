#include <gtest/gtest.h>

#include <vector>

#include "GpsPoint.h"
#include "filter/IGpsFilter.h"
#include "output/IOutput.h"
#include "parser/INmeaParser.h"

// ---------------------------------------------------------------------------
// GpsPoint
// ---------------------------------------------------------------------------

TEST(GpsPointTest, DefaultInitializesNumericFieldsToZero)
{
    GpsPoint p{};
    EXPECT_DOUBLE_EQ(p.lat,       0.0);
    EXPECT_DOUBLE_EQ(p.lon,       0.0);
    EXPECT_DOUBLE_EQ(p.speed_kmh, 0.0);
    EXPECT_DOUBLE_EQ(p.course,    0.0);
    EXPECT_DOUBLE_EQ(p.altitude,  0.0);
    EXPECT_EQ       (p.satellites, 0);
    EXPECT_DOUBLE_EQ(p.hdop,      0.0);
}

TEST(GpsPointTest, DefaultInitializesStringAndBoolFields)
{
    GpsPoint p{};
    EXPECT_TRUE (p.time.empty());
    EXPECT_FALSE(p.stopped);
}

TEST(GpsPointTest, FieldsAreWritable)
{
    GpsPoint p{};
    p.lat        = 55.7521;
    p.lon        = 37.6595;
    p.speed_kmh  = 46.3;
    p.course     = 45.0;
    p.altitude   = 150.0;
    p.satellites = 10;
    p.hdop       = 0.8;
    p.time       = "120000";
    p.stopped    = true;

    EXPECT_DOUBLE_EQ(p.lat,       55.7521);
    EXPECT_DOUBLE_EQ(p.lon,       37.6595);
    EXPECT_DOUBLE_EQ(p.speed_kmh, 46.3);
    EXPECT_DOUBLE_EQ(p.course,    45.0);
    EXPECT_DOUBLE_EQ(p.altitude,  150.0);
    EXPECT_EQ       (p.satellites, 10);
    EXPECT_DOUBLE_EQ(p.hdop,      0.8);
    EXPECT_EQ       (p.time,      "120000");
    EXPECT_TRUE     (p.stopped);
}

// ---------------------------------------------------------------------------
// FilterResult
// ---------------------------------------------------------------------------

TEST(FilterResultTest, PassStatusAndEmptyReason)
{
    FilterResult r{FilterStatus::Pass, ""};
    EXPECT_EQ  (r.status, FilterStatus::Pass);
    EXPECT_TRUE(r.reason.empty());
}

TEST(FilterResultTest, RejectStatusWithReason)
{
    FilterResult r{FilterStatus::Reject, "insufficient satellites (3 < 4)"};
    EXPECT_EQ(r.status, FilterStatus::Reject);
    EXPECT_EQ(r.reason, "insufficient satellites (3 < 4)");
}

// ---------------------------------------------------------------------------
// IGpsFilter — проверяем, что интерфейс реализуем и вызываем
// ---------------------------------------------------------------------------

namespace {

class AlwaysPassFilter final : public IGpsFilter
{
public:
    FilterResult apply(const GpsPoint&) override
    {
        return {FilterStatus::Pass, ""};
    }
};

class AlwaysRejectFilter final : public IGpsFilter
{
public:
    FilterResult apply(const GpsPoint&) override
    {
        return {FilterStatus::Reject, "always rejected"};
    }
};

} // namespace

TEST(IGpsFilterTest, PassImplementation)
{
    AlwaysPassFilter f;
    GpsPoint p{};
    const auto result = f.apply(p);
    EXPECT_EQ(result.status, FilterStatus::Pass);
    EXPECT_TRUE(result.reason.empty());
}

TEST(IGpsFilterTest, RejectImplementation)
{
    AlwaysRejectFilter f;
    GpsPoint p{};
    const auto result = f.apply(p);
    EXPECT_EQ(result.status, FilterStatus::Reject);
    EXPECT_EQ(result.reason, "always rejected");
}

// ---------------------------------------------------------------------------
// IOutput — проверяем все три метода интерфейса
// ---------------------------------------------------------------------------

namespace {

struct RecordingOutput final : public IOutput
{
    std::vector<std::string> log;

    void writePoint(const GpsPoint&) override
    {
        log.push_back("point");
    }
    void writeRejected(const GpsPoint&, const std::string& reason) override
    {
        log.push_back("rejected:" + reason);
    }
    void writeError(const std::string& message) override
    {
        log.push_back("error:" + message);
    }
};

} // namespace

TEST(IOutputTest, AllThreeMethodsAreCallable)
{
    RecordingOutput out;
    GpsPoint p{};
    out.writePoint(p);
    out.writeRejected(p, "bad data");
    out.writeError("checksum fail");

    ASSERT_EQ(out.log.size(), 3u);
    EXPECT_EQ(out.log[0], "point");
    EXPECT_EQ(out.log[1], "rejected:bad data");
    EXPECT_EQ(out.log[2], "error:checksum fail");
}

// ---------------------------------------------------------------------------
// INmeaParser — проверяем, что интерфейс реализуем
// ---------------------------------------------------------------------------

namespace {

class NullParser final : public INmeaParser
{
public:
    std::optional<GpsPoint> parse(const std::string&) override
    {
        return std::nullopt;
    }
};

} // namespace

TEST(INmeaParserTest, ReturnsNulloptForUnknownInput)
{
    NullParser parser;
    const auto result = parser.parse("$UNKNOWN,data*00");
    EXPECT_FALSE(result.has_value());
}
