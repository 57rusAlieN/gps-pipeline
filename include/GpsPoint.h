#pragma once

#include <string>

struct GpsPoint
{
    double lat        = 0.0;  // decimal degrees, positive = North
    double lon        = 0.0;  // decimal degrees, positive = East
    double speed_kmh  = 0.0;  // speed in km/h
    double course     = 0.0;  // true course, degrees [0, 360)
    double altitude   = 0.0;  // metres above mean sea level
    int    satellites = 0;
    double hdop       = 0.0;
    std::string time;         // HHMMSS[.ss] taken directly from NMEA sentence
    bool   stopped    = false; // set to true by StopFilter
};
