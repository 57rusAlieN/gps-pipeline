#include "filter/CoordinateJumpFilter.h"

#include <cmath>
#include <cstdio>

CoordinateJumpFilter::CoordinateJumpFilter(double maxDistanceM)
    : m_maxDistM{maxDistanceM}
{}

FilterResult CoordinateJumpFilter::process(GpsPoint& point)
{
    if (!m_prev.has_value())
    {
        m_prev = point;
        return {FilterStatus::Pass, ""};
    }

    const double dist = haversineDistanceM(m_prev->lat, m_prev->lon,
                                           point.lat,   point.lon);

    if (dist > m_maxDistM)
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf),
                      "coordinate jump detected (%.1f m > %.1f m)",
                      dist, m_maxDistM);
        return {FilterStatus::Reject, buf};
    }

    m_prev = point;
    return {FilterStatus::Pass, ""};
}

double CoordinateJumpFilter::haversineDistanceM(double lat1, double lon1,
                                                double lat2, double lon2)
{
    constexpr double R    = 6'371'000.0;
    constexpr double kPi  = 3.14159265358979323846;
    constexpr double kD2R = kPi / 180.0;

    const double phi1 = lat1 * kD2R;
    const double phi2 = lat2 * kD2R;
    const double dphi = (lat2 - lat1) * kD2R;
    const double dlam = (lon2 - lon1) * kD2R;

    const double a = std::sin(dphi / 2) * std::sin(dphi / 2)
                   + std::cos(phi1) * std::cos(phi2)
                   * std::sin(dlam / 2) * std::sin(dlam / 2);

    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return R * c;
}
