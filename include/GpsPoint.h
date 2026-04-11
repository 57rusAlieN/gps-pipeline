#pragma once

#include <string>
#include <vector>

// Per-satellite data from GPGSV sentences.
struct SatelliteInfo
{
    int prn       = 0;  // satellite PRN number
    int elevation = 0;  // degrees above horizon [0, 90]
    int azimuth   = 0;  // degrees from true north [0, 360)
    int snr       = 0;  // signal/noise ratio, dBHz (0 = not tracked)
};

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

    // Populated from GPGSV sentences (empty if no GSV received)
    std::vector<SatelliteInfo> satellites_in_view;
};
