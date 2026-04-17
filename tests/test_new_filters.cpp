#include <gtest/gtest.h>

#include "filter/QualityFilter.h"
#include "filter/HeightFilter.h"
#include "filter/JumpSuppressFilter.h"
#include "filter/ParkingFilter.h"

// ===========================================================================
// QualityFilter
// ===========================================================================

class QualityFilterTest : public ::testing::Test
{
protected:
    GpsPoint makePoint(double hdop = 1.0)
    {
        GpsPoint p;
        p.hdop = hdop;
        return p;
    }

    GpsPoint makePointWithSats(double hdop, std::vector<int> snrs)
    {
        GpsPoint p;
        p.hdop = hdop;
        for (int snr : snrs)
            p.satellites_in_view.push_back({0, 0, 0, snr});
        return p;
    }
};

TEST_F(QualityFilterTest, GoodHdop_Pass)
{
    QualityFilter f(5.0, 19);
    auto p = makePoint(2.0);
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(QualityFilterTest, BadHdop_Reject)
{
    QualityFilter f(5.0, 19);
    auto p = makePoint(5.0);  // >= 5.0
    EXPECT_EQ(f.process(p).status, FilterStatus::Reject);
}

TEST_F(QualityFilterTest, HdopJustBelow_Pass)
{
    QualityFilter f(5.0, 19);
    auto p = makePoint(4.99);
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(QualityFilterTest, EnoughUsableSats_Pass)
{
    QualityFilter f(10.0, 19);
    auto p = makePointWithSats(1.0, {25, 30, 20, 15});  // 3 usable
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(QualityFilterTest, InsufficientUsableSats_Reject)
{
    QualityFilter f(10.0, 19);
    auto p = makePointWithSats(1.0, {10, 12, 15, 18});  // 0 usable
    EXPECT_EQ(f.process(p).status, FilterStatus::Reject);
}

TEST_F(QualityFilterTest, NoSatView_SkipSnrCheck)
{
    QualityFilter f(10.0, 19);
    auto p = makePoint(1.0);  // no satellites_in_view
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(QualityFilterTest, ExactlyMinUsableSats_Pass)
{
    QualityFilter f(10.0, 19);
    auto p = makePointWithSats(1.0, {19, 20, 25, 5, 10});  // 3 usable
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(QualityFilterTest, TwoUsableSats_Reject)
{
    QualityFilter f(10.0, 19);
    auto p = makePointWithSats(1.0, {19, 25, 5, 10});  // 2 usable
    EXPECT_EQ(f.process(p).status, FilterStatus::Reject);
}

// ===========================================================================
// HeightFilter
// ===========================================================================

class HeightFilterTest : public ::testing::Test
{
protected:
    GpsPoint makePoint(double alt)
    {
        GpsPoint p;
        p.altitude = alt;
        return p;
    }
};

TEST_F(HeightFilterTest, InRange_Pass)
{
    HeightFilter f(-50, 2000, 50);
    auto p = makePoint(150);
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(HeightFilterTest, BelowMin_Reject)
{
    HeightFilter f(-50, 2000, 50);
    auto p = makePoint(-51);
    EXPECT_EQ(f.process(p).status, FilterStatus::Reject);
}

TEST_F(HeightFilterTest, AboveMax_Reject)
{
    HeightFilter f(-50, 2000, 50);
    auto p = makePoint(2001);
    EXPECT_EQ(f.process(p).status, FilterStatus::Reject);
}

TEST_F(HeightFilterTest, AltitudeJump_Reject)
{
    HeightFilter f(-50, 2000, 50);
    auto p1 = makePoint(100);
    f.process(p1);  // establish baseline
    auto p2 = makePoint(160);  // jump = 60 > 50
    EXPECT_EQ(f.process(p2).status, FilterStatus::Reject);
}

TEST_F(HeightFilterTest, AltitudeJumpExact_Pass)
{
    HeightFilter f(-50, 2000, 50);
    auto p1 = makePoint(100);
    f.process(p1);
    auto p2 = makePoint(150);  // jump = 50, exactly threshold
    EXPECT_EQ(f.process(p2).status, FilterStatus::Pass);
}

TEST_F(HeightFilterTest, FirstPoint_NoJumpCheck)
{
    HeightFilter f(-50, 2000, 50);
    auto p = makePoint(1500);
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

// ===========================================================================
// JumpSuppressFilter
// ===========================================================================

class JumpSuppressFilterTest : public ::testing::Test
{
protected:
    GpsPoint makePoint(double speedKmh)
    {
        GpsPoint p;
        p.speed_kmh = speedKmh;
        return p;
    }
};

TEST_F(JumpSuppressFilterTest, FirstPoint_AlwaysPass)
{
    JumpSuppressFilter f(6.0, 20.0, 5);
    auto p = makePoint(100);
    EXPECT_EQ(f.process(p).status, FilterStatus::Pass);
}

TEST_F(JumpSuppressFilterTest, SmallAcceleration_Pass)
{
    JumpSuppressFilter f(6.0, 20.0, 5);
    auto p1 = makePoint(36.0);  // 10 m/s
    f.process(p1);
    auto p2 = makePoint(50.4);  // 14 m/s, deltaV = 4 m/s < 6
    EXPECT_EQ(f.process(p2).status, FilterStatus::Pass);
}

TEST_F(JumpSuppressFilterTest, ExcessiveAcceleration_Reject)
{
    JumpSuppressFilter f(6.0, 20.0, 5);
    auto p1 = makePoint(36.0);  // 10 m/s
    f.process(p1);
    auto p2 = makePoint(61.2);  // 17 m/s, deltaV = 7 > 6
    EXPECT_EQ(f.process(p2).status, FilterStatus::Reject);
}

TEST_F(JumpSuppressFilterTest, VelocityJump_Reject)
{
    JumpSuppressFilter f(100.0, 20.0, 5);  // high acc limit
    auto p1 = makePoint(0);
    f.process(p1);
    auto p2 = makePoint(76.0);  // 21.1 m/s, deltaV > 20
    EXPECT_EQ(f.process(p2).status, FilterStatus::Reject);
}

TEST_F(JumpSuppressFilterTest, MaxWrong_ForceAccept)
{
    JumpSuppressFilter f(6.0, 20.0, 3);  // max 3 wrong
    auto p1 = makePoint(0);
    f.process(p1);  // baseline

    // 3 consecutive rejects (large jumps)
    auto bad = makePoint(200);
    EXPECT_EQ(f.process(bad).status, FilterStatus::Reject);  // 1
    EXPECT_EQ(f.process(bad).status, FilterStatus::Reject);  // 2
    EXPECT_EQ(f.process(bad).status, FilterStatus::Reject);  // 3

    // After 3 wrongs, next should be force-accepted
    EXPECT_EQ(f.process(bad).status, FilterStatus::Pass);
}

// ===========================================================================
// ParkingFilter
// ===========================================================================

class ParkingFilterTest : public ::testing::Test
{
protected:
    GpsPoint makePoint(double lat, double lon, double speed)
    {
        GpsPoint p;
        p.lat = lat; p.lon = lon; p.speed_kmh = speed;
        p.altitude = 100;
        return p;
    }
};

TEST_F(ParkingFilterTest, MovingPoint_Pass_NoFreeze)
{
    ParkingFilter f(4.0);
    auto p = makePoint(55.0, 37.0, 50.0);
    f.process(p);
    EXPECT_DOUBLE_EQ(p.lat, 55.0);
    EXPECT_FALSE(p.stopped);
}

TEST_F(ParkingFilterTest, SlowPoint_Stopped_CoordsFrozen)
{
    ParkingFilter f(4.0);
    auto p1 = makePoint(55.0, 37.0, 2.0);  // parked
    f.process(p1);
    EXPECT_TRUE(p1.stopped);

    auto p2 = makePoint(55.001, 37.001, 1.0);  // still parked, drifted
    f.process(p2);
    EXPECT_TRUE(p2.stopped);
    EXPECT_DOUBLE_EQ(p2.lat, 55.0);   // frozen to first parked pos
    EXPECT_DOUBLE_EQ(p2.lon, 37.0);
}

TEST_F(ParkingFilterTest, ResumesAfterMoving)
{
    ParkingFilter f(4.0);
    auto p1 = makePoint(55.0, 37.0, 2.0);
    f.process(p1);  // parked

    auto p2 = makePoint(55.1, 37.1, 50.0);  // moving again
    f.process(p2);
    EXPECT_FALSE(p2.stopped);
    EXPECT_DOUBLE_EQ(p2.lat, 55.1);  // not frozen
}

TEST_F(ParkingFilterTest, ReParking_NewFreezePosition)
{
    ParkingFilter f(4.0);
    auto p1 = makePoint(55.0, 37.0, 2.0);
    f.process(p1);  // parked at 55/37

    auto p2 = makePoint(55.1, 37.1, 50.0);
    f.process(p2);  // moving

    auto p3 = makePoint(56.0, 38.0, 1.0);
    f.process(p3);  // parked again at new location
    EXPECT_DOUBLE_EQ(p3.lat, 56.0);
    EXPECT_DOUBLE_EQ(p3.lon, 38.0);

    auto p4 = makePoint(56.001, 38.001, 0.5);
    f.process(p4);
    EXPECT_DOUBLE_EQ(p4.lat, 56.0);  // frozen to new parked pos
}
