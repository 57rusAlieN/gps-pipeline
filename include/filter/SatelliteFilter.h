#pragma once

#include "filter/IGpsFilter.h"

class SatelliteFilter final : public IGpsFilter
{
public:
    explicit SatelliteFilter(int minSatellites = 4);
    FilterResult process(GpsPoint& point) override;

private:
    int m_min;
};
