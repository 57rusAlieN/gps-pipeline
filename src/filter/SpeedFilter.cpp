#include "filter/SpeedFilter.h"

#include <format>

SpeedFilter::SpeedFilter(double maxSpeedKmh)
    : m_max{maxSpeedKmh}
{}

FilterResult SpeedFilter::process(GpsPoint& point)
{
    if (point.speed_kmh > m_max)
        return {FilterStatus::Reject,
                std::format("speed too high ({:.1f} > {:.1f} km/h)",
                            point.speed_kmh, m_max)};
    return {FilterStatus::Pass, ""};
}
