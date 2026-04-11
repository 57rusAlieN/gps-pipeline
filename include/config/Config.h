#pragma once

#include <string>
#include <cstddef>

// ---------------------------------------------------------------------------
// Config structs — all fields have sensible defaults.
// Fields mirror the JSON schema in config/default.json.
// ---------------------------------------------------------------------------

struct SatelliteFilterCfg
{
    bool enabled       = true;
    int  min_satellites = 4;
};

struct SpeedFilterCfg
{
    bool   enabled       = true;
    double max_speed_kmh = 200.0;
};

struct JumpFilterCfg
{
    bool   enabled       = true;
    double max_distance_m = 500.0;
};

struct StopFilterCfg
{
    bool   enabled       = true;
    double threshold_kmh = 3.0;
};

struct LpfFilterCfg
{
    bool        enabled = false;
    std::string type    = "pif";  // "pif" | "fir"
    double      cutoff  = 0.2;
};

struct FiltersCfg
{
    SatelliteFilterCfg satellite;
    SpeedFilterCfg     speed;
    JumpFilterCfg      jump;
    StopFilterCfg      stop;
    LpfFilterCfg       lpf;
};

struct RotationCfg
{
    std::size_t max_size_kb = 1024;  // 0 = no size limit
    int         max_files   = 5;    // 0 = no rotation
};

struct OutputCfg
{
    std::string type = "console";         // "console" | "file"
    std::string path = "gps_output.log";  // used when type == "file"
    RotationCfg rotation;
};

struct Config
{
    FiltersCfg filters;
    OutputCfg  output;
};
