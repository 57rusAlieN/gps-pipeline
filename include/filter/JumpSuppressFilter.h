#pragma once

#include "filter/IGpsFilter.h"

#include <optional>

// Rejects points with impossible acceleration or velocity jumps.
//
// From FILTER_CONFIG.md (GPS.CORRECT):
//   maxAcc    — max implied acceleration (m/s²)
//   maxJump   — max single-step velocity change (m/s)
//   maxWrong  — after this many consecutive rejects, accept the next point
//               unconditionally (forced reset to avoid permanent lockout)
class JumpSuppressFilter final : public IGpsFilter
{
public:
    JumpSuppressFilter(double maxAccMs2 = 6.0, double maxJumpMs = 20.0, int maxWrong = 5);
    FilterResult process(GpsPoint& point) override;

private:
    double m_maxAccMs2;
    double m_maxJumpMs;
    int    m_maxWrong;

    std::optional<double> m_prevSpeedMs;  // previous speed in m/s
    int m_wrongCount = 0;                 // consecutive reject counter
};
