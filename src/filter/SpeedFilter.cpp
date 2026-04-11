#include "filter/SpeedFilter.h"

#include <cstdio>

SpeedFilter::SpeedFilter(double maxSpeedKmh)
    : m_max{maxSpeedKmh}
{}

FilterResult SpeedFilter::process(GpsPoint& point)
{
    if (point.speed_kmh > m_max)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "speed too high (%.1f > %.1f km/h)",
                      point.speed_kmh, m_max);
        return {FilterStatus::Reject, buf};
    }
    return {FilterStatus::Pass, ""};
}
