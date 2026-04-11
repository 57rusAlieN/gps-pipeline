#include "filter/SatelliteFilter.h"

#include <string>

SatelliteFilter::SatelliteFilter(int minSatellites)
    : m_min{minSatellites}
{}

FilterResult SatelliteFilter::process(GpsPoint& point)
{
    if (point.satellites < m_min)
        return {FilterStatus::Reject,
                "insufficient satellites (" +
                std::to_string(point.satellites) + " < " +
                std::to_string(m_min) + ")"};
    return {FilterStatus::Pass, ""};
}
