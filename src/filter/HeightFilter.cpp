#include "filter/HeightFilter.h"

#include <cmath>

HeightFilter::HeightFilter(double minM, double maxM, double maxJumpM)
    : m_minM{minM}
    , m_maxM{maxM}
    , m_maxJumpM{maxJumpM}
{}

FilterResult HeightFilter::process(GpsPoint& point)
{
    if (point.altitude < m_minM)
        return {FilterStatus::Reject, "height: below minimum"};

    if (point.altitude > m_maxM)
        return {FilterStatus::Reject, "height: above maximum"};

    if (m_prevAlt.has_value())
    {
        if (std::abs(point.altitude - *m_prevAlt) > m_maxJumpM)
            return {FilterStatus::Reject, "height: altitude jump"};
    }

    m_prevAlt = point.altitude;
    return {FilterStatus::Pass, ""};
}
