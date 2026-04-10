#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "GpsPoint.h"
#include "output/ConsoleOutput.h"

// ---------------------------------------------------------------------------
// ConsoleOutput writes to any std::ostream; tests capture via std::ostringstream
// ---------------------------------------------------------------------------

namespace {

GpsPoint makePoint(const std::string& time = "120000",
                   double lat  = 55.752057, double lon = 37.659463,
                   double speedKmh = 46.3, double course = 45.0,
                   double altitude = 150.0, int satellites = 10,
                   double hdop = 0.8, bool stopped = false)
{
    GpsPoint p;
    p.time       = time;
    p.lat        = lat;
    p.lon        = lon;
    p.speed_kmh  = speedKmh;
    p.course     = course;
    p.altitude   = altitude;
    p.satellites = satellites;
    p.hdop       = hdop;
    p.stopped    = stopped;
    return p;
}

class ConsoleOutputTest : public ::testing::Test
{
protected:
    std::ostringstream oss;
    ConsoleOutput out{oss};
};

} // namespace

// ===========================================================================
// writePoint
// ===========================================================================

TEST_F(ConsoleOutputTest, WritePointFormatsTimestamp)
{
    // NMEA time "120000" → "[12:00:00]"
    out.writePoint(makePoint("120000"));
    EXPECT_NE(oss.str().find("[12:00:00]"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsCoordinates)
{
    out.writePoint(makePoint());
    const auto s = oss.str();
    EXPECT_NE(s.find("55.75206"), std::string::npos);
    EXPECT_NE(s.find("37.65946"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsNorthEastSuffix)
{
    out.writePoint(makePoint());
    const auto s = oss.str();
    EXPECT_NE(s.find("N"), std::string::npos);
    EXPECT_NE(s.find("E"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsSpeed)
{
    out.writePoint(makePoint());
    EXPECT_NE(oss.str().find("46.3"), std::string::npos);
    EXPECT_NE(oss.str().find("km/h"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsCourse)
{
    out.writePoint(makePoint());
    EXPECT_NE(oss.str().find("45.0"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsAltitude)
{
    out.writePoint(makePoint());
    EXPECT_NE(oss.str().find("150.0"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointContainsSatellitesAndHdop)
{
    out.writePoint(makePoint());
    const auto s = oss.str();
    EXPECT_NE(s.find("10"), std::string::npos);
    EXPECT_NE(s.find("0.8"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointStoppedMarker)
{
    // Stopped point → output must mention "stopped"
    out.writePoint(makePoint("120001", 55.752058, 37.659465, 2.2, 45.0,
                             150.0, 10, 0.8, true));
    EXPECT_NE(oss.str().find("stopped"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointNotStoppedOmitsMarker)
{
    out.writePoint(makePoint());
    // No "stopped" in normal-speed output
    EXPECT_EQ(oss.str().find("stopped"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WritePointSouthWestNegativeCoordinates)
{
    auto p = makePoint(); p.lat = -33.8688; p.lon = -70.6693;
    out.writePoint(p);
    const auto s = oss.str();
    EXPECT_NE(s.find("S"), std::string::npos);
    EXPECT_NE(s.find("W"), std::string::npos);
}

// ===========================================================================
// writeRejected
// ===========================================================================

TEST_F(ConsoleOutputTest, WriteRejectedFormatsTimestamp)
{
    out.writeRejected(makePoint("120002"), "coordinate jump detected");
    EXPECT_NE(oss.str().find("[12:00:02]"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WriteRejectedContainsReason)
{
    out.writeRejected(makePoint("120003"), "insufficient satellites (3 < 4)");
    const auto s = oss.str();
    EXPECT_NE(s.find("rejected"), std::string::npos);
    EXPECT_NE(s.find("insufficient satellites"), std::string::npos);
}

// ===========================================================================
// writeError
// ===========================================================================

TEST_F(ConsoleOutputTest, WriteErrorContainsMessage)
{
    out.writeError("Parse error: invalid checksum");
    const auto s = oss.str();
    EXPECT_NE(s.find("invalid checksum"), std::string::npos);
}

TEST_F(ConsoleOutputTest, WriteErrorNoGpsFix)
{
    out.writeError("[12:00:04] No valid GPS fix");
    EXPECT_NE(oss.str().find("No valid GPS fix"), std::string::npos);
}

// ===========================================================================
// IOutput interface contract
// ===========================================================================

TEST_F(ConsoleOutputTest, ImplementsIOutputInterface)
{
    IOutput* iface = &out;
    auto p = makePoint();
    iface->writePoint(p);
    iface->writeRejected(p, "test");
    iface->writeError("err");
    // All three methods callable via interface pointer
    EXPECT_FALSE(oss.str().empty());
}
