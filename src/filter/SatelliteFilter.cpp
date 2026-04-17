#include "filter/SatelliteFilter.h"

#include <string>

SatelliteFilter::SatelliteFilter(int minCount, int startCount, int waitSeconds)
    : m_minCount{minCount}
    , m_startCount{startCount}
    , m_waitPoints{waitSeconds}  // 1 Hz ≈ 1 point/second
{}

FilterResult SatelliteFilter::process(GpsPoint& point)
{
    ++m_pointsSeen;

    // Grace period: reject everything
    if (m_pointsSeen <= m_waitPoints)
        return {FilterStatus::Reject, "satellite: grace period"};

    // Require startCount to begin emitting
    if (!m_started)
    {
        if (point.satellites >= m_startCount)
            m_started = true;
        else
            return {FilterStatus::Reject,
                    "insufficient satellites (" +
                    std::to_string(point.satellites) + " < " +
                    std::to_string(m_startCount) + " start)"};
    }

    // Ongoing minimum
    if (point.satellites < m_minCount)
        return {FilterStatus::Reject,
                "insufficient satellites (" +
                std::to_string(point.satellites) + " < " +
                std::to_string(m_minCount) + ")"};

    return {FilterStatus::Pass, ""};
}
