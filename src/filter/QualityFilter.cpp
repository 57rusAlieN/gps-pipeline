#include "filter/QualityFilter.h"

QualityFilter::QualityFilter(double maxHdop, int minSnr)
    : m_maxHdop{maxHdop}
    , m_minSnr{minSnr}
{}

FilterResult QualityFilter::process(GpsPoint& point)
{
    // Check HDOP
    if (point.hdop >= m_maxHdop)
        return {FilterStatus::Reject, "quality: HDOP too high"};

    // Count usable satellites (SNR >= minSnr)
    if (!point.satellites_in_view.empty())
    {
        int usable = 0;
        for (const auto& sat : point.satellites_in_view)
        {
            if (sat.snr >= m_minSnr)
                ++usable;
        }
        if (usable < MIN_USABLE_SATS)
            return {FilterStatus::Reject, "quality: insufficient usable satellites"};
    }

    return {FilterStatus::Pass, ""};
}
