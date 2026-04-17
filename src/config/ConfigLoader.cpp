#include "config/ConfigLoader.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers — extract value if key present, keep default otherwise
// ---------------------------------------------------------------------------
namespace {

template<typename T>
T get(const json& j, const char* key, T def)
{
    auto it = j.find(key);
    return (it != j.end() && !it->is_null()) ? it->get<T>() : def;
}

SatelliteFilterCfg parseSatellite(const json& j, const SatelliteFilterCfg& def)
{
    SatelliteFilterCfg c = def;
    c.enabled        = get(j, "enabled",        def.enabled);
    c.min_satellites = get(j, "min_satellites", def.min_satellites);
    c.start_count   = get(j, "start_count",    def.start_count);
    c.wait_seconds  = get(j, "wait_seconds",   def.wait_seconds);
    return c;
}

QualityFilterCfg parseQuality(const json& j, const QualityFilterCfg& def)
{
    QualityFilterCfg c = def;
    c.enabled  = get(j, "enabled",  def.enabled);
    c.max_hdop = get(j, "max_hdop", def.max_hdop);
    c.min_snr  = get(j, "min_snr",  def.min_snr);
    return c;
}

SpeedFilterCfg parseSpeed(const json& j, const SpeedFilterCfg& def)
{
    SpeedFilterCfg c = def;
    c.enabled       = get(j, "enabled",       def.enabled);
    c.max_speed_kmh = get(j, "max_speed_kmh", def.max_speed_kmh);
    return c;
}

JumpFilterCfg parseJump(const json& j, const JumpFilterCfg& def)
{
    JumpFilterCfg c = def;
    c.enabled       = get(j, "enabled",       def.enabled);
    c.max_distance_m = get(j, "max_distance_m", def.max_distance_m);
    return c;
}

HeightFilterCfg parseHeight(const json& j, const HeightFilterCfg& def)
{
    HeightFilterCfg c = def;
    c.enabled    = get(j, "enabled",    def.enabled);
    c.min_m      = get(j, "min_m",      def.min_m);
    c.max_m      = get(j, "max_m",      def.max_m);
    c.max_jump_m = get(j, "max_jump_m", def.max_jump_m);
    return c;
}

JumpSuppressFilterCfg parseJumpSuppress(const json& j, const JumpSuppressFilterCfg& def)
{
    JumpSuppressFilterCfg c = def;
    c.enabled     = get(j, "enabled",     def.enabled);
    c.max_acc_ms2 = get(j, "max_acc_ms2", def.max_acc_ms2);
    c.max_jump_ms = get(j, "max_jump_ms", def.max_jump_ms);
    c.max_wrong   = get(j, "max_wrong",   def.max_wrong);
    return c;
}

StopFilterCfg parseStop(const json& j, const StopFilterCfg& def)
{
    StopFilterCfg c = def;
    c.enabled       = get(j, "enabled",       def.enabled);
    c.threshold_kmh = get(j, "threshold_kmh", def.threshold_kmh);
    return c;
}

ParkingFilterCfg parseParking(const json& j, const ParkingFilterCfg& def)
{
    ParkingFilterCfg c = def;
    c.enabled   = get(j, "enabled",   def.enabled);
    c.speed_kmh = get(j, "speed_kmh", def.speed_kmh);
    return c;
}

LpfFilterCfg parseLpf(const json& j, const LpfFilterCfg& def)
{
    LpfFilterCfg c = def;
    c.enabled = get(j, "enabled", def.enabled);
    c.type    = get(j, "type",    def.type);
    c.cutoff  = get(j, "cutoff",  def.cutoff);
    return c;
}

FiltersCfg parseFilters(const json& j, const FiltersCfg& def)
{
    FiltersCfg c = def;
    if (auto it = j.find("satellite");     it != j.end()) c.satellite     = parseSatellite    (*it, def.satellite);
    if (auto it = j.find("quality");       it != j.end()) c.quality       = parseQuality      (*it, def.quality);
    if (auto it = j.find("speed");         it != j.end()) c.speed         = parseSpeed        (*it, def.speed);
    if (auto it = j.find("height");        it != j.end()) c.height        = parseHeight       (*it, def.height);
    if (auto it = j.find("jump");          it != j.end()) c.jump          = parseJump         (*it, def.jump);
    if (auto it = j.find("jump_suppress"); it != j.end()) c.jump_suppress = parseJumpSuppress (*it, def.jump_suppress);
    if (auto it = j.find("stop");          it != j.end()) c.stop          = parseStop         (*it, def.stop);
    if (auto it = j.find("parking");       it != j.end()) c.parking       = parseParking      (*it, def.parking);
    if (auto it = j.find("lpf");           it != j.end()) c.lpf           = parseLpf          (*it, def.lpf);
    return c;
}

RotationCfg parseRotation(const json& j, const RotationCfg& def)
{
    RotationCfg c = def;
    if (auto it = j.find("max_size_kb"); it != j.end()) c.max_size_kb = it->get<std::size_t>();
    if (auto it = j.find("max_files");   it != j.end()) c.max_files   = it->get<int>();
    return c;
}

OutputCfg parseOutput(const json& j, const OutputCfg& def)
{
    OutputCfg c = def;
    c.type = get(j, "type", def.type);
    c.path = get(j, "path", def.path);
    if (auto it = j.find("rotation"); it != j.end()) c.rotation = parseRotation(*it, def.rotation);
    return c;
}

InputCfg parseInput(const json& j, const InputCfg& def)
{
    InputCfg c = def;
    c.type      = get(j, "type",      def.type);
    c.path      = get(j, "path",      def.path);
    c.recursive = get(j, "recursive", def.recursive);
    return c;
}

Config fromJson(const json& j)
{
    Config c;  // all defaults via struct initialisation
    if (auto it = j.find("input");   it != j.end()) c.input   = parseInput  (*it, Config{}.input);
    if (auto it = j.find("filters"); it != j.end()) c.filters = parseFilters(*it, Config{}.filters);
    if (auto it = j.find("output");  it != j.end()) c.output  = parseOutput (*it, Config{}.output);
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Config ConfigLoader::defaults()
{
    return Config{};
}

Config ConfigLoader::loadString(const std::string& jsonStr)
{
    try
    {
        return fromJson(json::parse(jsonStr));
    }
    catch (const json::exception& e)
    {
        throw std::runtime_error(std::string("Config parse error: ") + e.what());
    }
}

Config ConfigLoader::loadFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot open file '" + path + "'");

    try
    {
        return fromJson(json::parse(f));
    }
    catch (const json::exception& e)
    {
        throw std::runtime_error("Config '" + path + "': " + e.what());
    }
}
