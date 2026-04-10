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
    virtual FilterResult apply(const GpsPoint& point) = 0;
    virtual ~IGpsFilter() = default;
};
