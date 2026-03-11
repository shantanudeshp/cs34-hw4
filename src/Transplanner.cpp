#include "TransportationPlannerConfig.h"
#include "DijkstraTransportationPlanner.h"
#include "TransportationPlannerCommandLine.h"
#include "OpenStreetMap.h"
#include "CSVBusSystem.h"
#include "DSVReader.h"
#include "XMLReader.h"
#include "FileDataFactory.h"
#include "FileDataSource.h"
#include "FileDataSink.h"
#include "StandardDataSource.h"
#include "StandardDataSink.h"
#include "StandardErrorDataSink.h"
#include "StringUtils.h"
#include <iostream>

int main(int argc, char *argv[]) {
    std::string DataDirectory   = "./data";
    std::string ResultsDirectory = "./results";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--data=") == 0) {
            DataDirectory = arg.substr(7);
        } else if (arg.find("--results=") == 0) {
            ResultsDirectory = arg.substr(10);
        } else {
            std::cerr << "Syntax: transplanner [--data=path] [--results=path]" << std::endl;
            return EXIT_FAILURE;
        }
    }

    const std::string OSMFilename   = "city.osm";
    const std::string StopFilename  = "stops.csv";
    const std::string RouteFilename = "routes.csv";

    auto DataFactory    = std::make_shared<CFileDataFactory>(DataDirectory);
    auto ResultsFactory = std::make_shared<CFileDataFactory>(ResultsDirectory);
    auto StdIn  = std::make_shared<CStandardDataSource>();
    auto StdOut = std::make_shared<CStandardDataSink>();
    auto StdErr = std::make_shared<CStandardErrorDataSink>();

    auto StopReader  = std::make_shared<CDSVReader>(DataFactory->CreateSource(StopFilename), ',');
    auto RouteReader = std::make_shared<CDSVReader>(DataFactory->CreateSource(RouteFilename), ',');
    auto BusSystem   = std::make_shared<CCSVBusSystem>(StopReader, RouteReader);

    auto XMLReader  = std::make_shared<CXMLReader>(DataFactory->CreateSource(OSMFilename));
    auto StreetMap  = std::make_shared<COpenStreetMap>(XMLReader);

    auto Config  = std::make_shared<STransportationPlannerConfig>(StreetMap, BusSystem);
    auto Planner = std::make_shared<CDijkstraTransportationPlanner>(Config);

    CTransportationPlannerCommandLine CommandLine(StdIn, StdOut, StdErr, ResultsFactory, Planner);
    CommandLine.ProcessCommands();

    return EXIT_SUCCESS;
}