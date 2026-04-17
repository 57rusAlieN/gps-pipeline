#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#  include <fcntl.h>
#  include <io.h>
#endif

#include "filter/FilterChain.h"
#include "filter/SatelliteFilter.h"
#include "filter/QualityFilter.h"
#include "filter/SpeedFilter.h"
#include "filter/HeightFilter.h"
#include "filter/CoordinateJumpFilter.h"
#include "filter/JumpSuppressFilter.h"
#include "filter/StopFilter.h"
#include "filter/ParkingFilter.h"
#include "filter/MovingAverageFilter.h"
#include "filter/FirLowPassFilter.h"
#include "output/ConsoleOutput.h"
#include "output/FileOutput.h"
#include "output/IOutput.h"
#include "config/Config.h"
#include "config/ConfigLoader.h"
#include "pipeline/Pipeline.h"
#include "pipeline/MultiFileRecordReader.h"

static void printUsage()
{
    std::cerr <<
        "Usage: gps_pipeline <path> [--config <cfg.json>]\n"
        "                          [-lpf <pif|fir>] [-cf <0.01..1>]\n"
        "  <path>    file (.nmea/.bin) OR directory to scan recursively\n"
        "  --config  JSON config file; overrides -lpf/-cf flags\n"
        "  -lpf      smoothing: pif (moving average) or fir (windowed-sinc)\n"
        "  -cf       low-pass cutoff [0.01..1], 1=no smoothing; default: 0.2\n";
}

enum class CliOpt { File, Config, Lpf, Cf, Unknown };

static CliOpt classifyArg(const char* s) noexcept
{
    if (s[0] != '-')                    return CliOpt::File;
    if (std::strcmp(s, "--config") == 0) return CliOpt::Config;
    if (std::strcmp(s, "-lpf")     == 0) return CliOpt::Lpf;
    if (std::strcmp(s, "-cf")      == 0) return CliOpt::Cf;
    return CliOpt::Unknown;
}

static void buildFilters(FilterChain& filters, const Config& cfg)
{
    const auto& f = cfg.filters;

    // Order matches FILTER_CONFIG.md pipeline order:
    // 1. SatellitesFilter → 2. QualityFilter → 3. SpeedFilter →
    // 4. HeightFilter → 5. JumpSuppressFilter → 6. ParkingFilter
    // Legacy filters (jump, stop, lpf) appended after if enabled.

    if (f.satellite.enabled)
        filters.add(std::make_unique<SatelliteFilter>(
            f.satellite.min_satellites,
            f.satellite.start_count,
            f.satellite.wait_seconds));

    if (f.quality.enabled)
        filters.add(std::make_unique<QualityFilter>(
            f.quality.max_hdop, f.quality.min_snr));

    if (f.speed.enabled)
        filters.add(std::make_unique<SpeedFilter>(f.speed.max_speed_kmh));

    if (f.height.enabled)
        filters.add(std::make_unique<HeightFilter>(
            f.height.min_m, f.height.max_m, f.height.max_jump_m));

    if (f.jump.enabled)
        filters.add(std::make_unique<CoordinateJumpFilter>(f.jump.max_distance_m));

    if (f.jump_suppress.enabled)
        filters.add(std::make_unique<JumpSuppressFilter>(
            f.jump_suppress.max_acc_ms2,
            f.jump_suppress.max_jump_ms,
            f.jump_suppress.max_wrong));

    if (f.stop.enabled)
        filters.add(std::make_unique<StopFilter>(f.stop.threshold_kmh));

    if (f.parking.enabled)
        filters.add(std::make_unique<ParkingFilter>(f.parking.speed_kmh));

    if (f.lpf.enabled && f.lpf.cutoff < 1.0)
    {
        if (f.lpf.type == "fir")
            filters.add(std::make_unique<FirLowPassFilter>(f.lpf.cutoff, 21));
        else
        {
            const int w = std::max(3, static_cast<int>(std::round(0.886 / f.lpf.cutoff)));
            filters.add(std::make_unique<MovingAverageFilter>(w));
        }
    }
}

static std::unique_ptr<IOutput> buildOutput(const Config& cfg)
{
    if (cfg.output.type == "file")
        return std::make_unique<FileOutput>(
            cfg.output.path,
            cfg.output.rotation.max_size_kb,
            cfg.output.rotation.max_files);

    return std::make_unique<ConsoleOutput>(std::cout);
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    std::string filePath;
    std::string configPath;
    std::string lpfType    = "pif";
    double      cutoffNorm = 0.2;

    for (int i = 1; i < argc; ++i)
    {
        switch (classifyArg(argv[i]))
        {
        case CliOpt::File:
            if (!filePath.empty())
            {
                std::cerr << "Error: unexpected argument '" << argv[i] << "'\n";
                printUsage();
                return EXIT_FAILURE;
            }
            filePath = argv[i];
            break;

        case CliOpt::Config:
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --config requires a file path\n";
                return EXIT_FAILURE;
            }
            configPath = argv[++i];
            break;

        case CliOpt::Lpf:
            if (i + 1 >= argc)
            {
                std::cerr << "Error: -lpf requires an argument (pif or fir)\n";
                return EXIT_FAILURE;
            }
            lpfType = argv[++i];
            if (lpfType != "pif" && lpfType != "fir")
            {
                std::cerr << "Error: -lpf must be 'pif' or 'fir', got '" << lpfType << "'\n";
                return EXIT_FAILURE;
            }
            break;

        case CliOpt::Cf:
            if (i + 1 >= argc)
            {
                std::cerr << "Error: -cf requires a numeric argument\n";
                return EXIT_FAILURE;
            }
            try   { cutoffNorm = std::stod(argv[++i]); }
            catch (...)
            {
                std::cerr << "Error: -cf value '" << argv[i] << "' is not a number\n";
                return EXIT_FAILURE;
            }
            if (cutoffNorm < 0.01 || cutoffNorm > 1.0)
            {
                std::cerr << "Error: -cf must be in [0.01, 1.0], got " << cutoffNorm << "\n";
                return EXIT_FAILURE;
            }
            break;

        case CliOpt::Unknown:
        default:
            std::cerr << "Error: unknown argument '" << argv[i] << "'\n";
            printUsage();
            return EXIT_FAILURE;
        }
    }

    if (filePath.empty() && configPath.empty())
    {
        printUsage();
        return EXIT_FAILURE;
    }

    // Build config (--config overrides all CLI filter flags)
    Config cfg;
    if (!configPath.empty())
    {
        try   { cfg = ConfigLoader::loadFile(configPath); }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        cfg.filters.lpf.enabled = (cutoffNorm < 1.0);
        cfg.filters.lpf.type    = lpfType;
        cfg.filters.lpf.cutoff  = cutoffNorm;
    }

    // CLI path overrides config input.path (if provided)
    if (!filePath.empty())
    {
        cfg.input.path      = filePath;
        cfg.input.recursive = false;
        const bool isBinary = (filePath.size() >= 4 &&
                                filePath.compare(filePath.size() - 4, 4, ".bin") == 0);
        if (cfg.input.type == "auto")
            cfg.input.type = isBinary ? "binary" : "nmea";
    }

    // Composition root
    FilterChain filters;
    buildFilters(filters, cfg);

    std::unique_ptr<IOutput> output;
    try   { output = buildOutput(cfg); }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto reader = MultiFileRecordReader::fromDirectory(
            cfg.input.path, cfg.input.recursive, cfg.input.type);

        Pipeline pipeline{reader, filters, *output};
        pipeline.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
