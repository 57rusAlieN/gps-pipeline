#pragma once

#include <optional>
#include <string>

#include "GpsPoint.h"

class INmeaParser
{
public:
    virtual std::optional<GpsPoint> parse(const std::string& line) = 0;
    virtual ~INmeaParser() = default;
};
