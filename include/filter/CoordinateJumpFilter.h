#pragma once

#include "filter/IGpsFilter.h"

#include <optional>

class CoordinateJumpFilter final : public IGpsFilter
{
public:
    explicit CoordinateJumpFilter(double maxDistanceM = 500.0);
    FilterResult process(GpsPoint& point) override;

private:
    double m_maxDistM;
    std::optional<GpsPoint> m_prev;

    static double haversineDistanceM(double lat1, double lon1,
                                     double lat2, double lon2);
};
