#include "filter/FilterChain.h"

void FilterChain::add(std::unique_ptr<IGpsFilter> filter)
{
    m_filters.push_back(std::move(filter));
}

FilterResult FilterChain::process(GpsPoint& point)
{
    for (auto& f : m_filters)
    {
        auto result = f->process(point);
        if (result.status == FilterStatus::Reject)
            return result;
    }
    return {FilterStatus::Pass, ""};
}
