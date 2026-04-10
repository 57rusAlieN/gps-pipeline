#pragma once

#include <string>

#include "GpsPoint.h"

class IOutput
{
public:
    virtual void writePoint   (const GpsPoint& point)                          = 0;
    virtual void writeRejected(const GpsPoint& point, const std::string& reason) = 0;
    virtual void writeError   (const std::string& message)                     = 0;
    virtual ~IOutput() = default;
};
