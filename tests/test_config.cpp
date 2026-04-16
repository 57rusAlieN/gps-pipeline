#include <gtest/gtest.h>

#include "config/Config.h"
#include "config/ConfigLoader.h"

// ===========================================================================
// Defaults
// ===========================================================================

TEST(ConfigLoaderTest, DefaultsHaveExpectedValues)
{
    Config c = ConfigLoader::defaults();

    EXPECT_TRUE (c.filters.satellite.enabled);
    EXPECT_EQ   (c.filters.satellite.min_satellites, 4);

    EXPECT_TRUE (c.filters.speed.enabled);
    EXPECT_DOUBLE_EQ(c.filters.speed.max_speed_kmh, 200.0);

    EXPECT_TRUE (c.filters.jump.enabled);
    EXPECT_DOUBLE_EQ(c.filters.jump.max_distance_m, 500.0);

    EXPECT_TRUE (c.filters.stop.enabled);
    EXPECT_DOUBLE_EQ(c.filters.stop.threshold_kmh, 3.0);

    EXPECT_FALSE(c.filters.lpf.enabled);
    EXPECT_EQ   (c.filters.lpf.type, "pif");
    EXPECT_DOUBLE_EQ(c.filters.lpf.cutoff, 0.2);

    EXPECT_EQ(c.output.type, "console");
}

// ===========================================================================
// Empty JSON → all defaults
// ===========================================================================

TEST(ConfigLoaderTest, EmptyJsonGivesDefaults)
{
    Config c = ConfigLoader::loadString("{}");
    EXPECT_EQ(c.filters.satellite.min_satellites, 4);
    EXPECT_EQ(c.output.type, "console");
}

// ===========================================================================
// Filter overrides
// ===========================================================================

TEST(ConfigLoaderTest, OverrideMinSatellites)
{
    Config c = ConfigLoader::loadString(R"({"filters":{"satellite":{"min_satellites":6}}})");
    EXPECT_EQ(c.filters.satellite.min_satellites, 6);
    EXPECT_TRUE(c.filters.satellite.enabled);  // default preserved
}

TEST(ConfigLoaderTest, DisableSatelliteFilter)
{
    Config c = ConfigLoader::loadString(R"({"filters":{"satellite":{"enabled":false}}})");
    EXPECT_FALSE(c.filters.satellite.enabled);
    EXPECT_EQ(c.filters.satellite.min_satellites, 4);  // default preserved
}

TEST(ConfigLoaderTest, OverrideSpeedFilter)
{
    Config c = ConfigLoader::loadString(R"({"filters":{"speed":{"max_speed_kmh":120.5}}})");
    EXPECT_DOUBLE_EQ(c.filters.speed.max_speed_kmh, 120.5);
}

TEST(ConfigLoaderTest, OverrideJumpFilter)
{
    Config c = ConfigLoader::loadString(R"({"filters":{"jump":{"max_distance_m":300.0}}})");
    EXPECT_DOUBLE_EQ(c.filters.jump.max_distance_m, 300.0);
}

TEST(ConfigLoaderTest, OverrideStopFilter)
{
    Config c = ConfigLoader::loadString(R"({"filters":{"stop":{"threshold_kmh":5.0}}})");
    EXPECT_DOUBLE_EQ(c.filters.stop.threshold_kmh, 5.0);
}

TEST(ConfigLoaderTest, EnableLpfFir)
{
    Config c = ConfigLoader::loadString(
        R"({"filters":{"lpf":{"enabled":true,"type":"fir","cutoff":0.15}}})");
    EXPECT_TRUE(c.filters.lpf.enabled);
    EXPECT_EQ  (c.filters.lpf.type, "fir");
    EXPECT_DOUBLE_EQ(c.filters.lpf.cutoff, 0.15);
}

// ===========================================================================
// Output overrides
// ===========================================================================

TEST(ConfigLoaderTest, OutputConsoleDefault)
{
    Config c = ConfigLoader::loadString("{}");
    EXPECT_EQ(c.output.type, "console");
}

TEST(ConfigLoaderTest, OutputFileWithPath)
{
    Config c = ConfigLoader::loadString(
        R"({"output":{"type":"file","path":"out.log"}})");
    EXPECT_EQ(c.output.type, "file");
    EXPECT_EQ(c.output.path, "out.log");
}

TEST(ConfigLoaderTest, OutputFileWithRotation)
{
    Config c = ConfigLoader::loadString(R"({
        "output": {
            "type": "file",
            "path": "gps.log",
            "rotation": { "max_size_kb": 512, "max_files": 3 }
        }
    })");
    EXPECT_EQ(c.output.type, "file");
    EXPECT_EQ(c.output.rotation.max_size_kb, 512u);
    EXPECT_EQ(c.output.rotation.max_files,   3);
}

// ===========================================================================
// Invalid JSON
// ===========================================================================

TEST(ConfigLoaderTest, InvalidJsonThrows)
{
    EXPECT_THROW(ConfigLoader::loadString("{bad json}"), std::runtime_error);
}

// ===========================================================================
// loadFile: non-existent file throws
// ===========================================================================

TEST(ConfigLoaderTest, LoadNonExistentFileThrows)
{
    EXPECT_THROW(ConfigLoader::loadFile("no_such_file.json"), std::runtime_error);
}

// ===========================================================================
// InputCfg — defaults
// ===========================================================================

TEST(ConfigLoaderTest, InputCfgDefaultType)
{
    Config c = ConfigLoader::defaults();
    EXPECT_EQ(c.input.type, "auto");
}

TEST(ConfigLoaderTest, InputCfgDefaultPath)
{
    Config c = ConfigLoader::defaults();
    EXPECT_EQ(c.input.path, "");
}

TEST(ConfigLoaderTest, InputCfgDefaultRecursive)
{
    Config c = ConfigLoader::defaults();
    EXPECT_FALSE(c.input.recursive);
}

TEST(ConfigLoaderTest, EmptyJsonInputCfgKeepsDefaults)
{
    Config c = ConfigLoader::loadString("{}");
    EXPECT_EQ(c.input.type, "auto");
    EXPECT_EQ(c.input.path, "");
    EXPECT_FALSE(c.input.recursive);
}

// ===========================================================================
// InputCfg — overrides
// ===========================================================================

TEST(ConfigLoaderTest, InputCfgTypeNmea)
{
    Config c = ConfigLoader::loadString(R"({"input":{"type":"nmea"}})");
    EXPECT_EQ(c.input.type, "nmea");
}

TEST(ConfigLoaderTest, InputCfgTypeBinary)
{
    Config c = ConfigLoader::loadString(R"({"input":{"type":"binary"}})");
    EXPECT_EQ(c.input.type, "binary");
}

TEST(ConfigLoaderTest, InputCfgPath)
{
    Config c = ConfigLoader::loadString(R"({"input":{"path":"data/260317"}})");
    EXPECT_EQ(c.input.path, "data/260317");
}

TEST(ConfigLoaderTest, InputCfgRecursiveTrue)
{
    Config c = ConfigLoader::loadString(R"({"input":{"recursive":true}})");
    EXPECT_TRUE(c.input.recursive);
}

TEST(ConfigLoaderTest, InputCfgFullSection)
{
    Config c = ConfigLoader::loadString(R"({
        "input": {
            "type":      "binary",
            "path":      "data/260317",
            "recursive": true
        }
    })");
    EXPECT_EQ(c.input.type,      "binary");
    EXPECT_EQ(c.input.path,      "data/260317");
    EXPECT_TRUE(c.input.recursive);
}

TEST(ConfigLoaderTest, InputCfgPartialOverride)
{
    Config c = ConfigLoader::loadString(R"({"input":{"type":"nmea"}})");
    EXPECT_EQ  (c.input.type, "nmea");
    EXPECT_EQ  (c.input.path, "");        // default preserved
    EXPECT_FALSE(c.input.recursive);      // default preserved
}

