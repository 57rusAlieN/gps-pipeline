#pragma once

#include "filter/IGpsFilter.h"

class SpeedFilter final : public IGpsFilter
{
public:
    explicit SpeedFilter(double maxSpeedKmh = 200.0);
    FilterResult process(GpsPoint& point) override;

private:
    double m_max;
};
