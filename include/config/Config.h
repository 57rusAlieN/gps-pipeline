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
    int  min_satellites = 4;     // ongoing minimum (satMinCount)
    int  start_count   = 4;     // required to start emitting (satStartCount)
    int  wait_seconds  = 0;     // grace period after boot — reject all points (satWaitTime)
};

struct QualityFilterCfg
{
    bool   enabled  = false;
    double max_hdop = 5.0;     // reject if HDOP >= this value
    int    min_snr  = 19;      // minimum per-satellite SNR (dB-Hz) to count as usable
};

struct SpeedFilterCfg
{
    bool   enabled       = true;
    double max_speed_kmh = 200.0;
};

struct HeightFilterCfg
{
    bool   enabled    = false;
    double min_m      = -50.0;   // reject if altitude < min
    double max_m      = 2000.0;  // reject if altitude > max
    double max_jump_m = 50.0;    // reject if |alt - prev_alt| > max_jump
};

struct JumpFilterCfg
{
    bool   enabled       = true;
    double max_distance_m = 500.0;
};

struct JumpSuppressFilterCfg
{
    bool   enabled      = false;
    double max_acc_ms2  = 6.0;   // max implied acceleration (m/s²)
    double max_jump_ms  = 20.0;  // max single-step velocity change (m/s)
    int    max_wrong    = 5;     // after this many consecutive rejects, accept next
};

struct StopFilterCfg
{
    bool   enabled       = true;
    double threshold_kmh = 3.0;
};

struct ParkingFilterCfg
{
    bool   enabled   = false;
    double speed_kmh = 4.0;  // freeze position when speed < this
};

struct LpfFilterCfg
{
    bool        enabled = false;
    std::string type    = "pif";  // "pif" | "fir"
    double      cutoff  = 0.2;
};

struct FiltersCfg
{
    SatelliteFilterCfg    satellite;
    QualityFilterCfg      quality;
    SpeedFilterCfg        speed;
    HeightFilterCfg       height;
    JumpFilterCfg         jump;
    JumpSuppressFilterCfg jump_suppress;
    StopFilterCfg         stop;
    ParkingFilterCfg      parking;
    LpfFilterCfg          lpf;
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

// ---------------------------------------------------------------------------
// Input configuration — what to read and how to scan
// ---------------------------------------------------------------------------

struct InputCfg
{
    // "auto"   — detect by file extension (.bin → binary, else nmea)
    // "nmea"   — force NMEA text parser
    // "binary" — force GnssBinaryParser
    std::string type      = "auto";

    // Path to a single file OR a root directory to scan
    std::string path      = "";

    // Recursively descend into subdirectories when path is a directory.
    // Files are processed in lexicographic (= chronological for YYMMDD/HH/MM.bin)
    // order relative to the root.
    bool        recursive = false;
};

struct Config
{
    InputCfg   input;
    FiltersCfg filters;
    OutputCfg  output;
};
