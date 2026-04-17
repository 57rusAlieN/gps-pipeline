#pragma once

#include "filter/IGpsFilter.h"

#include <optional>

// Freezes position while the vehicle is parked (speed < parkingSpeed).
//
// Unlike StopFilter (which only sets a flag), ParkingFilter replaces lat/lon
// with the last known "parked" position and sets stopped=true.
// When speed rises above threshold, normal tracking resumes.
class ParkingFilter final : public IGpsFilter
{
public:
    explicit ParkingFilter(double parkingSpeedKmh = 4.0);
    FilterResult process(GpsPoint& point) override;

private:
    double m_parkingSpeed;

    // First point where speed dropped below threshold
    std::optional<GpsPoint> m_parkedPos;
};
