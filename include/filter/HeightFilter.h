#pragma once

#include "filter/IGpsFilter.h"

#include <optional>

// Rejects points with out-of-range altitude or altitude jumps.
//
// From FILTER_CONFIG.md:
//   - Reject if altitude < minM or altitude > maxM
//   - Reject if |altitude - prev_altitude| > maxJumpM
class HeightFilter final : public IGpsFilter
{
public:
    HeightFilter(double minM = -50.0, double maxM = 2000.0, double maxJumpM = 50.0);
    FilterResult process(GpsPoint& point) override;

private:
    double m_minM;
    double m_maxM;
    double m_maxJumpM;
    std::optional<double> m_prevAlt;
};
