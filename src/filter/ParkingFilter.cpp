#include "filter/ParkingFilter.h"

ParkingFilter::ParkingFilter(double parkingSpeedKmh)
    : m_parkingSpeed{parkingSpeedKmh}
{}

FilterResult ParkingFilter::process(GpsPoint& point)
{
    if (point.speed_kmh < m_parkingSpeed)
    {
        // Enter or stay in parked state
        if (!m_parkedPos.has_value())
            m_parkedPos = point;  // freeze this position

        // Replace coordinates with parked position
        point.lat      = m_parkedPos->lat;
        point.lon      = m_parkedPos->lon;
        point.altitude = m_parkedPos->altitude;
        point.stopped  = true;
        return {FilterStatus::Pass, ""};
    }

    // Moving — exit parked state
    m_parkedPos.reset();
    return {FilterStatus::Pass, ""};
}
