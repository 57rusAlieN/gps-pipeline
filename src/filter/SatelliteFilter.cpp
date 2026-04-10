#include "filter/SatelliteFilter.h"

#include <format>

SatelliteFilter::SatelliteFilter(int minSatellites)
    : m_min{minSatellites}
{}

FilterResult SatelliteFilter::process(GpsPoint& point)
{
    if (point.satellites < m_min)
        return {FilterStatus::Reject,
                std::format("insufficient satellites ({} < {})",
                            point.satellites, m_min)};
    return {FilterStatus::Pass, ""};
}
