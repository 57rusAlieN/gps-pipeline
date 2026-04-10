#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "GpsPoint.h"
#include "filter/SatelliteFilter.h"
#include "filter/SpeedFilter.h"
#include "filter/CoordinateJumpFilter.h"
#include "filter/StopFilter.h"
#include "filter/FilterChain.h"
#include "filter/MovingAverageFilter.h"
#include "filter/FirLowPassFilter.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

GpsPoint makePoint(double lat = 55.7521, double lon = 37.6595,
                   double speedKmh = 46.3,  int satellites = 10,
                   double altitude = 150.0)
{
    GpsPoint p;
    p.lat        = lat;
    p.lon        = lon;
    p.speed_kmh  = speedKmh;
    p.satellites = satellites;
    p.altitude   = altitude;
    p.hdop       = 0.8;
    p.course     = 45.0;
    p.time       = "120000";
    return p;
}

// Coordinates from data/sample.nmea (verified)
constexpr double kBaseLat = 55.752057;   // t=120000
constexpr double kBaseLon = 37.659463;
constexpr double kNearLat = 55.752058;   // t=120001, dist ≈ 0.2 m
constexpr double kNearLon = 37.659465;
constexpr double kFarLat  = 55.766667;   // t=120002, dist ≈ 1893 m
constexpr double kFarLon  = 37.675000;

} // namespace

// ===========================================================================
// SatelliteFilter
// ===========================================================================

class SatelliteFilterTest : public ::testing::Test
{
protected:
    SatelliteFilter filter;   // default min = 4
};

TEST_F(SatelliteFilterTest, PassWhenAboveMin)
{
    auto p = makePoint(); p.satellites = 10;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(SatelliteFilterTest, RejectWhenBelowMin)
{
    auto p = makePoint(); p.satellites = 3;
    const auto r = filter.process(p);
    EXPECT_EQ(r.status, FilterStatus::Reject);
}

TEST_F(SatelliteFilterTest, PassAtExactMin)
{
    auto p = makePoint(); p.satellites = 4;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(SatelliteFilterTest, RejectReasonMentionsCount)
{
    auto p = makePoint(); p.satellites = 3;
    const auto r = filter.process(p);
    // Expected: "insufficient satellites (3 < 4)"
    EXPECT_NE(r.reason.find("3"), std::string::npos);
    EXPECT_NE(r.reason.find("4"), std::string::npos);
}

TEST_F(SatelliteFilterTest, CustomMinValue)
{
    SatelliteFilter strict(6);
    auto p = makePoint(); p.satellites = 5;
    EXPECT_EQ(strict.process(p).status, FilterStatus::Reject);
    p.satellites = 6;
    EXPECT_EQ(strict.process(p).status, FilterStatus::Pass);
}

// ===========================================================================
// SpeedFilter
// ===========================================================================

class SpeedFilterTest : public ::testing::Test
{
protected:
    SpeedFilter filter;   // default max = 200 km/h
};

TEST_F(SpeedFilterTest, PassNormalSpeed)
{
    auto p = makePoint(); p.speed_kmh = 90.0;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(SpeedFilterTest, RejectAboveMax)
{
    auto p = makePoint(); p.speed_kmh = 250.0;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Reject);
}

TEST_F(SpeedFilterTest, PassAtExactMax)
{
    auto p = makePoint(); p.speed_kmh = 200.0;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(SpeedFilterTest, ZeroSpeedAlwaysPasses)
{
    auto p = makePoint(); p.speed_kmh = 0.0;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(SpeedFilterTest, CustomMaxValue)
{
    SpeedFilter strict(60.0);
    auto p = makePoint(); p.speed_kmh = 61.0;
    EXPECT_EQ(strict.process(p).status, FilterStatus::Reject);
    p.speed_kmh = 60.0;
    EXPECT_EQ(strict.process(p).status, FilterStatus::Pass);
}

// ===========================================================================
// CoordinateJumpFilter
// ===========================================================================

class CoordinateJumpFilterTest : public ::testing::Test
{
protected:
    CoordinateJumpFilter filter{500.0};  // 500 m threshold
};

TEST_F(CoordinateJumpFilterTest, FirstPointAlwaysPasses)
{
    auto p = makePoint(kFarLat, kFarLon);   // even a "far" point passes as first
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(CoordinateJumpFilterTest, SmallMovementPasses)
{
    auto base = makePoint(kBaseLat, kBaseLon);
    auto near = makePoint(kNearLat, kNearLon);
    filter.process(base);
    EXPECT_EQ(filter.process(near).status, FilterStatus::Pass);
}

TEST_F(CoordinateJumpFilterTest, LargeJumpRejected)
{
    auto base = makePoint(kBaseLat, kBaseLon);
    auto far  = makePoint(kFarLat,  kFarLon);   // ~1893 m
    filter.process(base);
    EXPECT_EQ(filter.process(far).status, FilterStatus::Reject);
}

TEST_F(CoordinateJumpFilterTest, RejectReasonMentionsJump)
{
    auto base = makePoint(kBaseLat, kBaseLon);
    auto far  = makePoint(kFarLat,  kFarLon);
    filter.process(base);
    const auto r = filter.process(far);
    EXPECT_NE(r.reason.find("jump"), std::string::npos);
}

TEST_F(CoordinateJumpFilterTest, PrevNotUpdatedOnReject)
{
    // base stored, jump rejected → prev remains base → near from base passes
    auto base = makePoint(kBaseLat, kBaseLon);
    auto far  = makePoint(kFarLat,  kFarLon);
    auto near = makePoint(kNearLat, kNearLon);
    filter.process(base);
    filter.process(far);                           // rejected, prev = base
    EXPECT_EQ(filter.process(near).status, FilterStatus::Pass);
}

TEST_F(CoordinateJumpFilterTest, DefaultThreshold)
{
    CoordinateJumpFilter def;               // default 500 m
    auto base = makePoint(kBaseLat, kBaseLon);
    auto far  = makePoint(kFarLat,  kFarLon);
    def.process(base);
    EXPECT_EQ(def.process(far).status, FilterStatus::Reject);
}

// ===========================================================================
// StopFilter
// ===========================================================================

class StopFilterTest : public ::testing::Test
{
protected:
    StopFilter filter;   // default threshold = 2.0 km/h
};

TEST_F(StopFilterTest, FastSpeedAlwaysPasses)
{
    auto p = makePoint(); p.speed_kmh = 46.3;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(StopFilterTest, SlowSpeedReturnsPas)
{
    auto p = makePoint(); p.speed_kmh = 1.0;
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);   // never rejects
}

TEST_F(StopFilterTest, SlowSpeedSetsStoppedFlag)
{
    auto p = makePoint(); p.speed_kmh = 1.0;
    filter.process(p);
    EXPECT_TRUE(p.stopped);
}

TEST_F(StopFilterTest, ZeroSpeedStopped)
{
    auto p = makePoint(); p.speed_kmh = 0.0;
    filter.process(p);
    EXPECT_TRUE(p.stopped);
}

TEST_F(StopFilterTest, FastSpeedDoesNotSetStoppedFlag)
{
    auto p = makePoint(); p.speed_kmh = 46.3;
    filter.process(p);
    EXPECT_FALSE(p.stopped);
}

TEST_F(StopFilterTest, CustomThreshold)
{
    StopFilter slow(5.0);
    auto p = makePoint(); p.speed_kmh = 4.9;
    slow.process(p);
    EXPECT_TRUE(p.stopped);
    p.stopped = false;
    p.speed_kmh = 5.0;
    slow.process(p);
    EXPECT_FALSE(p.stopped);
}

// ===========================================================================
// FilterChain
// ===========================================================================

namespace {

class SpyFilter final : public IGpsFilter
{
public:
    FilterStatus nextStatus = FilterStatus::Pass;
    int callCount = 0;

    FilterResult process(GpsPoint&) override
    {
        ++callCount;
        return {nextStatus, "spy"};
    }
};

} // namespace

TEST(FilterChainTest, EmptyChainReturnsPass)
{
    FilterChain chain;
    auto p = makePoint();
    EXPECT_EQ(chain.process(p).status, FilterStatus::Pass);
}

TEST(FilterChainTest, SinglePassFilterPasses)
{
    FilterChain chain;
    auto spy = std::make_unique<SpyFilter>();
    chain.add(std::move(spy));
    auto p = makePoint();
    EXPECT_EQ(chain.process(p).status, FilterStatus::Pass);
}

TEST(FilterChainTest, SingleRejectFilterRejects)
{
    FilterChain chain;
    auto spy = std::make_unique<SpyFilter>();
    spy->nextStatus = FilterStatus::Reject;
    chain.add(std::move(spy));
    auto p = makePoint();
    EXPECT_EQ(chain.process(p).status, FilterStatus::Reject);
}

TEST(FilterChainTest, StopsAtFirstReject)
{
    FilterChain chain;
    auto first  = std::make_unique<SpyFilter>();
    auto second = std::make_unique<SpyFilter>();
    first->nextStatus = FilterStatus::Reject;
    SpyFilter* rawSecond = second.get();
    chain.add(std::move(first));
    chain.add(std::move(second));
    auto p = makePoint();
    chain.process(p);
    EXPECT_EQ(rawSecond->callCount, 0);   // second filter never called
}

TEST(FilterChainTest, AllPassContinuesToEnd)
{
    FilterChain chain;
    auto f1 = std::make_unique<SpyFilter>();
    auto f2 = std::make_unique<SpyFilter>();
    SpyFilter* raw1 = f1.get();
    SpyFilter* raw2 = f2.get();
    chain.add(std::move(f1));
    chain.add(std::move(f2));
    auto p = makePoint();
    chain.process(p);
    EXPECT_EQ(raw1->callCount, 1);
    EXPECT_EQ(raw2->callCount, 1);
}

TEST(FilterChainTest, PointModifiedByEarlierFilterFlowsToLater)
{
    // StopFilter sets stopped=true; verify subsequent filter sees modified point
    FilterChain chain;
    chain.add(std::make_unique<StopFilter>(10.0));   // threshold 10 km/h

    auto p = makePoint(); p.speed_kmh = 5.0;
    chain.process(p);
    EXPECT_TRUE(p.stopped);
}

TEST(FilterChainTest, ImplementsIGpsFilterInterface)
{
    // FilterChain itself is an IGpsFilter (Composite pattern)
    FilterChain chain;
    IGpsFilter* f = &chain;
    auto p = makePoint();
    EXPECT_EQ(f->process(p).status, FilterStatus::Pass);
}

// ===========================================================================
// MovingAverageFilter  (ПИФ — прямоугольный импульсный фильтр)
// ===========================================================================

class MovingAverageFilterTest : public ::testing::Test
{
protected:
    MovingAverageFilter filter{3};   // window = 3
};

TEST_F(MovingAverageFilterTest, AlwaysReturnsPass)
{
    auto p = makePoint();
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(MovingAverageFilterTest, SingleValueUnchanged)
{
    // With "fill history on first sample", first output == first input
    auto p = makePoint(55.0, 37.0, 10.0);
    filter.process(p);
    EXPECT_NEAR(p.lat,       55.0, 1e-9);
    EXPECT_NEAR(p.lon,       37.0, 1e-9);
    EXPECT_NEAR(p.speed_kmh, 10.0, 1e-9);
}

TEST_F(MovingAverageFilterTest, ConstantInputUnchanged)
{
    auto p = makePoint(55.0, 37.0);
    for (int i = 0; i < 10; ++i) {
        p.lat = 55.0; p.lon = 37.0;
        filter.process(p);
    }
    EXPECT_NEAR(p.lat, 55.0, 1e-9);
    EXPECT_NEAR(p.lon, 37.0, 1e-9);
}

TEST_F(MovingAverageFilterTest, ConvergesCorrectlyWindow3)
{
    // history initialised with 1.0 on first call
    // n=0: fill=[1,1,1]    → lat = 1.0
    // n=1: buf=[1,1,2]     → lat = 4/3
    // n=2: buf=[1,2,3]     → lat = 6/3 = 2.0
    // n=3: buf=[2,3,4]     → lat = 9/3 = 3.0
    auto p = makePoint(); p.lat = 1.0;
    filter.process(p);  EXPECT_NEAR(p.lat, 1.0, 1e-9);
    p.lat = 2.0; filter.process(p);
    p.lat = 3.0; filter.process(p);  EXPECT_NEAR(p.lat, 2.0, 1e-9);
    p.lat = 4.0; filter.process(p);  EXPECT_NEAR(p.lat, 3.0, 1e-9);
}

TEST_F(MovingAverageFilterTest, SmoothesAllTargetFields)
{
    // Verify lat, lon, speed_kmh and altitude are all smoothed
    auto p = makePoint(10.0, 20.0, 30.0);
    p.altitude = 100.0;
    filter.process(p);  // fill
    p.lat = 40.0; p.lon = 50.0; p.speed_kmh = 60.0; p.altitude = 700.0;
    filter.process(p);
    // average of [10,10,40] = 20; [20,20,50] = 30; etc.
    EXPECT_NEAR(p.lat,       20.0, 1e-9);
    EXPECT_NEAR(p.lon,       30.0, 1e-9);
    EXPECT_NEAR(p.speed_kmh, 40.0, 1e-9);
    EXPECT_NEAR(p.altitude,  300.0, 1e-9);
}

TEST_F(MovingAverageFilterTest, LargerWindowSmoothesMore)
{
    MovingAverageFilter big(9);
    auto p = makePoint(); p.lat = 0.0;
    big.process(p);   // fill window-9 with 0

    // Feed 5 samples of 100 — fewer than window size, so history still has 0s
    // window-9: [0,0,0,0,100,100,100,100,100] → avg ≈ 55.6
    // window-3 (this.filter): already full of 100s after 3 samples → avg = 100
    for (int i = 0; i < 5; ++i) { p.lat = 100.0; big.process(p); }
    EXPECT_LT(p.lat, 100.0);   // window-9 lags behind window-3
}

// ===========================================================================
// FirLowPassFilter  (КИХ — конечная импульсная характеристика)
//   Windowed-sinc design, Hamming window
//   fc ∈ (0,1] — нормированная к частоте Найквиста
// ===========================================================================

class FirLowPassFilterTest : public ::testing::Test
{
protected:
    // fc=0.5, 21 taps — balanced LPF
    FirLowPassFilter filter{0.5, 21};
};

TEST_F(FirLowPassFilterTest, AlwaysReturnsPass)
{
    auto p = makePoint();
    EXPECT_EQ(filter.process(p).status, FilterStatus::Pass);
}

TEST_F(FirLowPassFilterTest, SingleValueUnchanged)
{
    // "fill with first value" → output equals first input
    auto p = makePoint(55.0, 37.0, 10.0);
    filter.process(p);
    EXPECT_NEAR(p.lat,       55.0, 1e-9);
    EXPECT_NEAR(p.lon,       37.0, 1e-9);
    EXPECT_NEAR(p.speed_kmh, 10.0, 1e-9);
}

TEST_F(FirLowPassFilterTest, ConstantInputUnchanged)
{
    // Normalised coefficients sum to 1 → constant signal passes unchanged
    auto p = makePoint(55.0, 37.0);
    for (int i = 0; i < 50; ++i) {
        p.lat = 55.0; p.lon = 37.0;
        filter.process(p);
    }
    EXPECT_NEAR(p.lat, 55.0, 1e-6);
    EXPECT_NEAR(p.lon, 37.0, 1e-6);
}

TEST_F(FirLowPassFilterTest, LowCutoffAttenuatesNyquistFrequency)
{
    // fc=0.05 (very low), feed alternating 0/10 (= Nyquist component)
    // After settling, output ≈ 5.0 (DC) ± 0.5
    FirLowPassFilter tight(0.05, 51);
    auto p = makePoint(); p.lat = 0.0;
    for (int i = 0; i < 200; ++i) {
        p.lat = (i % 2 == 0) ? 0.0 : 10.0;
        tight.process(p);
    }
    EXPECT_NEAR(p.lat, 5.0, 0.5);
}

TEST_F(FirLowPassFilterTest, HighCutoffPassesMoreFrequency)
{
    // fc=0.9 — near-allpass, alternating signal should keep larger amplitude
    FirLowPassFilter wide(0.9, 21);
    FirLowPassFilter tight(0.1, 21);
    auto pw = makePoint(); pw.lat = 0.0;
    auto pt = makePoint(); pt.lat = 0.0;
    for (int i = 0; i < 100; ++i) {
        pw.lat = pt.lat = (i % 2 == 0) ? 0.0 : 10.0;
        wide.process(pw);
        tight.process(pt);
    }
    // Wide cutoff → output closer to input extremes than tight cutoff
    const double ampWide  = std::abs(pw.lat - 5.0);
    const double ampTight = std::abs(pt.lat - 5.0);
    EXPECT_GT(ampWide, ampTight);
}

TEST_F(FirLowPassFilterTest, SmoothesAllTargetFields)
{
    auto p = makePoint(55.0, 37.0, 46.3);
    p.altitude = 150.0;
    // All fields initialised to same constant → should remain constant
    for (int i = 0; i < 30; ++i) filter.process(p);
    EXPECT_NEAR(p.lat,       55.0,  1e-6);
    EXPECT_NEAR(p.lon,       37.0,  1e-6);
    EXPECT_NEAR(p.speed_kmh, 46.3,  1e-6);
    EXPECT_NEAR(p.altitude,  150.0, 1e-6);
}
