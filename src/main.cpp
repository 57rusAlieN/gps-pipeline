#include <cmath>
#include <cstdlib>
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

#include "parser/NmeaParser.h"
#include "filter/FilterChain.h"
#include "filter/SatelliteFilter.h"
#include "filter/SpeedFilter.h"
#include "filter/CoordinateJumpFilter.h"
#include "filter/StopFilter.h"
#include "filter/MovingAverageFilter.h"
#include "filter/FirLowPassFilter.h"
#include "output/ConsoleOutput.h"
#include "pipeline/Pipeline.h"

static void printUsage()
{
    std::cerr <<
        "Usage: gps_pipeline <nmea_file> [-lpf <pif|fir>] [-cf <0..0.99>]\n"
        "  -lpf   smoothing filter type: pif (moving average) or fir (windowed-sinc)\n"
        "         default: pif\n"
        "  -cf    low-pass cutoff, Nyquist-normalised [0..0.99]\n"
        "         0 = no smoothing; default: 0.2\n";
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    // ---------------------------------------------------------------------------
    // Argument parsing
    // ---------------------------------------------------------------------------
    std::string filePath;
    std::string lpfType    = "pif";   // default: moving average
    double      cutoffNorm = 0.2;     // default: 0.2 × Nyquist

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-lpf")
        {
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
        }
        else if (arg == "-cf")
        {
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
            if (cutoffNorm < 0.0 || cutoffNorm > 0.99)
            {
                std::cerr << "Error: -cf must be in [0.0, 0.99], got " << cutoffNorm << "\n";
                return EXIT_FAILURE;
            }
        }
        else if (filePath.empty() && arg[0] != '-')
        {
            filePath = arg;
        }
        else
        {
            std::cerr << "Error: unknown argument '" << arg << "'\n";
            printUsage();
            return EXIT_FAILURE;
        }
    }

    if (filePath.empty())
    {
        printUsage();
        return EXIT_FAILURE;
    }

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: cannot open file '" << filePath << "'\n";
        return EXIT_FAILURE;
    }

    // ---------------------------------------------------------------------------
    // Composition root
    // ---------------------------------------------------------------------------
    NmeaParser parser;

    FilterChain filters;
    filters.add(std::make_unique<SatelliteFilter>(4));          // min 4 satellites
    filters.add(std::make_unique<SpeedFilter>(200.0));          // max 200 km/h
    filters.add(std::make_unique<CoordinateJumpFilter>(500.0)); // max 500 m jump
    filters.add(std::make_unique<StopFilter>(3.0));             // < 3 km/h -> stopped

    // Low-pass smoothing filter (after validation/correction filters)
    // cutoffNorm == 0 means "no smoothing"
    if (cutoffNorm > 0.0)
    {
        if (lpfType == "fir")
        {
            filters.add(std::make_unique<FirLowPassFilter>(cutoffNorm, 21));
        }
        else  // pif — moving average
        {
            // -3dB of box filter: fc_nyquist ≈ 0.886/N  =>  N ≈ 0.886/fc
            const int windowSize =
                std::max(3, static_cast<int>(std::round(0.886 / cutoffNorm)));
            filters.add(std::make_unique<MovingAverageFilter>(windowSize));
        }
    }

    ConsoleOutput output{std::cout};
    Pipeline pipeline{parser, filters, output};

    // ---------------------------------------------------------------------------
    // Process file line by line
    // ---------------------------------------------------------------------------
    std::string line;
    while (std::getline(file, line))
        pipeline.processLine(line);

    return EXIT_SUCCESS;
}