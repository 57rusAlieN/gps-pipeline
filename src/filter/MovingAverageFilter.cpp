#include "filter/MovingAverageFilter.h"

#include <numeric>

MovingAverageFilter::MovingAverageFilter(int windowSize)
    : m_windowSize{windowSize}
{}

FilterResult MovingAverageFilter::process(GpsPoint& point)
{
    point.lat       = filter(m_lat,   point.lat);
    point.lon       = filter(m_lon,   point.lon);
    point.speed_kmh = filter(m_speed, point.speed_kmh);
    point.altitude  = filter(m_alt,   point.altitude);
    return {FilterStatus::Pass, ""};
}

double MovingAverageFilter::filter(std::deque<double>& history, double x)
{
    if (history.empty())
    {
        // Pre-fill with first value to avoid initial transient
        history.assign(static_cast<std::size_t>(m_windowSize), x);
    }
    else
    {
        history.push_back(x);
        history.pop_front();
    }

    const double sum = std::accumulate(history.begin(), history.end(), 0.0);
    return sum / static_cast<double>(history.size());
}
