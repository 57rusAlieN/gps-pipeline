#pragma once

#include <string>

#include "GpsPoint.h"

enum class FilterStatus
{
    Pass,
    Reject
};

struct FilterResult
{
    FilterStatus status;
    std::string  reason;
};

class IGpsFilter
{
public:
    virtual FilterResult process(GpsPoint& point) = 0;
    virtual ~IGpsFilter() = default;
};
