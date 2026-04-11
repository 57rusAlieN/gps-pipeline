#pragma once

#include "config/Config.h"
#include <string>

// Loads a Config from a JSON file or JSON string.
// Missing keys keep their default values (Config{} initialises all defaults).
// Throws std::runtime_error on JSON parse errors.
class ConfigLoader
{
public:
    static Config loadFile  (const std::string& path);
    static Config loadString(const std::string& json);
    static Config defaults  ();
};
