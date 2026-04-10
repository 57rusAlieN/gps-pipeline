#pragma once

#include "filter/IGpsFilter.h"

#include <deque>

// Simple box (rectangular window) moving-average filter — ПИФ.
// Smoothes lat, lon, speed_kmh, altitude in-place.
// History is pre-filled with the first sample to avoid initial transient.
class MovingAverageFilter final : public IGpsFilter
{
public:
    explicit MovingAverageFilter(int windowSize = 5);
    FilterResult process(GpsPoint& point) override;

private:
    int m_windowSize;

    std::deque<double> m_lat;
    std::deque<double> m_lon;
    std::deque<double> m_speed;
    std::deque<double> m_alt;

    double filter(std::deque<double>& history, double x);
};
