#include "filter/StopFilter.h"

StopFilter::StopFilter(double thresholdKmh)
    : m_threshold{thresholdKmh}
{}

FilterResult StopFilter::process(GpsPoint& point)
{
    if (point.speed_kmh < m_threshold)
        point.stopped = true;
    return {FilterStatus::Pass, ""};
}
