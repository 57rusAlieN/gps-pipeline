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

StopFilterCfg parseStop(const json& j, const StopFilterCfg& def)
{
    StopFilterCfg c = def;
    c.enabled       = get(j, "enabled",       def.enabled);
    c.threshold_kmh = get(j, "threshold_kmh", def.threshold_kmh);
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
    if (auto it = j.find("satellite"); it != j.end()) c.satellite = parseSatellite(*it, def.satellite);
    if (auto it = j.find("speed");     it != j.end()) c.speed     = parseSpeed    (*it, def.speed);
    if (auto it = j.find("jump");      it != j.end()) c.jump      = parseJump     (*it, def.jump);
    if (auto it = j.find("stop");      it != j.end()) c.stop      = parseStop     (*it, def.stop);
    if (auto it = j.find("lpf");       it != j.end()) c.lpf       = parseLpf      (*it, def.lpf);
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

Config fromJson(const json& j)
{
    Config c;  // all defaults via struct initialisation
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
