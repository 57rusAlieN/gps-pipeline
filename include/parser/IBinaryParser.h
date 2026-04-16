#pragma once

#include "GpsPoint.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// Interface for parsers that convert a fixed-size binary record to a GpsPoint.
//
// Each binary record is exactly RECORD_SIZE bytes (packed, little-endian).
// If the record contains no valid GPS fix the implementation returns nullopt.
class IBinaryParser
{
public:
    static constexpr std::size_t RECORD_SIZE = 196;

    virtual std::optional<GpsPoint> parseRecord(const uint8_t* data,
                                                std::size_t    size) = 0;

    // NavStatus of the most recently parsed record — allows callers to
    // generate a meaningful error message when parseRecord returns nullopt.
    virtual uint8_t lastNavStatus() const noexcept = 0;

    virtual ~IBinaryParser() = default;
};
