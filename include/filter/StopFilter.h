#pragma once

#include "filter/IGpsFilter.h"

// Sets point.stopped = true when speed < threshold; always returns Pass.
class StopFilter final : public IGpsFilter
{
public:
    explicit StopFilter(double thresholdKmh = 2.0);
    FilterResult process(GpsPoint& point) override;

private:
    double m_threshold;
};
