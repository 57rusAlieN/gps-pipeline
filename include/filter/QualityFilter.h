#pragma once

#include "filter/IGpsFilter.h"

// Rejects points with poor HDOP or insufficient usable satellites (by SNR).
//
// From FILTER_CONFIG.md:
//   - Reject if HDOP >= maxHdop
//   - A satellite is "usable" when its SNR >= minSnr (dB-Hz)
//   - At least 3 usable satellites are required for LOW quality
//
// The full adaptive-threshold logic from the firmware is NOT implemented here;
// this is the configurable static subset.
class QualityFilter final : public IGpsFilter
{
public:
    QualityFilter(double maxHdop = 5.0, int minSnr = 19);
    FilterResult process(GpsPoint& point) override;

private:
    double m_maxHdop;
    int    m_minSnr;
    static constexpr int MIN_USABLE_SATS = 3;
};
