#pragma once

#include "filter/IGpsFilter.h"

// Rejects points until enough satellites are locked.
//
// From FILTER_CONFIG.md (GPS.CORRECT2):
//   wait_seconds — grace period after boot, reject ALL points
//   start_count  — minimum satellites to start emitting
//   min_count    — ongoing minimum after initial lock acquired
//
// At 1 Hz data rate, wait_seconds ≈ point count (used as proxy).
class SatelliteFilter final : public IGpsFilter
{
public:
    SatelliteFilter(int minCount = 4, int startCount = 4, int waitSeconds = 0);
    FilterResult process(GpsPoint& point) override;

private:
    int  m_minCount;
    int  m_startCount;
    int  m_waitPoints;    // grace period in points (≈ seconds at 1 Hz)
    int  m_pointsSeen = 0;
    bool m_started    = false;  // true once startCount was met after grace period
};
