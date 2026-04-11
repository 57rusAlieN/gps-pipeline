#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
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
#include "output/ConsoleOutput.h"
#include "pipeline/Pipeline.h"

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    if (argc != 2)
    {
        std::cerr << "Usage: gps_pipeline <nmea_file>\n";
        return EXIT_FAILURE;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open())
    {
        std::cerr << "Error: cannot open file '" << argv[1] << "'\n";
        return EXIT_FAILURE;
    }

    NmeaParser parser;

    FilterChain filters;
    filters.add(std::make_unique<SatelliteFilter>(4));
    filters.add(std::make_unique<SpeedFilter>(200.0));
    filters.add(std::make_unique<CoordinateJumpFilter>(500.0));
    filters.add(std::make_unique<StopFilter>(3.0));

    ConsoleOutput output{std::cout};
    Pipeline pipeline{parser, filters, output};

    std::string line;
    while (std::getline(file, line))
        pipeline.processLine(line);

    return EXIT_SUCCESS;
}