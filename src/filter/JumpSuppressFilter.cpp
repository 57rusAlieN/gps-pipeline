#include "filter/JumpSuppressFilter.h"

#include <cmath>

JumpSuppressFilter::JumpSuppressFilter(double maxAccMs2, double maxJumpMs, int maxWrong)
    : m_maxAccMs2{maxAccMs2}
    , m_maxJumpMs{maxJumpMs}
    , m_maxWrong{maxWrong}
{}

FilterResult JumpSuppressFilter::process(GpsPoint& point)
{
    const double currentMs = point.speed_kmh / 3.6;

    if (!m_prevSpeedMs.has_value())
    {
        // First point — accept unconditionally
        m_prevSpeedMs = currentMs;
        return {FilterStatus::Pass, ""};
    }

    const double deltaV = std::abs(currentMs - *m_prevSpeedMs);

    // After maxWrong consecutive rejects, force-accept (reset)
    if (m_wrongCount >= m_maxWrong)
    {
        m_wrongCount  = 0;
        m_prevSpeedMs = currentMs;
        return {FilterStatus::Pass, ""};
    }

    // Velocity jump check (delta-v in m/s)
    if (deltaV > m_maxJumpMs)
    {
        ++m_wrongCount;
        return {FilterStatus::Reject, "jump_suppress: velocity jump"};
    }

    // Acceleration check: deltaV / dt where dt ≈ 1 s (1 Hz data)
    // For sub-second data the firmware uses actual dt, but our binary dumps
    // are 1 Hz so dt=1 is appropriate.
    if (deltaV > m_maxAccMs2)
    {
        ++m_wrongCount;
        return {FilterStatus::Reject, "jump_suppress: excessive acceleration"};
    }

    m_wrongCount  = 0;
    m_prevSpeedMs = currentMs;
    return {FilterStatus::Pass, ""};
}
