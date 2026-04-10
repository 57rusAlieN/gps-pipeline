#pragma once

#include "filter/IGpsFilter.h"

#include <memory>
#include <vector>

// Composite filter: runs each child in order; stops on first Reject.
class FilterChain final : public IGpsFilter
{
public:
    void add(std::unique_ptr<IGpsFilter> filter);
    FilterResult process(GpsPoint& point) override;

private:
    std::vector<std::unique_ptr<IGpsFilter>> m_filters;
};
