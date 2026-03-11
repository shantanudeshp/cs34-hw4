#include "TransportationPlannerCommandLine.h"
#include "GeographicUtils.h"
#include "PathRouter.h"
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>

struct CTransportationPlannerCommandLine::SImplementation {
    std::shared_ptr<CDataSource>            CmdSrc;
    std::shared_ptr<CDataSink>              OutSink;
    std::shared_ptr<CDataSink>              ErrSink;
    std::shared_ptr<CDataFactory>           Results;
    std::shared_ptr<CTransportationPlanner> Planner;

    bool   HasPath      = false;
    bool   LastWasFastest = false;
    double LastTime     = 0.0;
    double LastDist     = 0.0;
    CTransportationPlanner::TNodeID LastSrc  = 0;
    CTransportationPlanner::TNodeID LastDest = 0;
    std::vector<CTransportationPlanner::TNodeID>   ShortPath;
    std::vector<CTransportationPlanner::TTripStep> FastPath;

    void Write(const std::string &s) {
        for (char c : s) OutSink->Put(c);
    }
    void WriteErr(const std::string &s) {
        for (char c : s) ErrSink->Put(c);
    }

    std::string ReadLine() {
        std::string line;
        char c;
        while (CmdSrc->Get(c)) {
            if (c == '\n') break;
            line += c;
        }
        return line;
    }

    static std::vector<std::string> Tokenize(const std::string &s) {
        std::vector<std::string> tokens;
        std::istringstream iss(s);
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        return tokens;
    }

    // Format time in hours to human-readable string
    // e.g. 0.65 hr -> "39 min"
    // e.g. 1.375 hr -> "1 hr 22 min 30 sec"
    static std::string FormatTime(double hours) {
        long long totalSecs = static_cast<long long>(std::round(hours * 3600.0));
        long long hrs  = totalSecs / 3600;
        long long mins = (totalSecs % 3600) / 60;
        long long secs = totalSecs % 60;

        std::ostringstream oss;
        if (hrs > 0) {
            oss << hrs << " hr";
            if (mins > 0 || secs > 0) {
                oss << " " << mins << " min";
                if (secs > 0) oss << " " << secs << " sec";
            }
        } else if (mins > 0) {
            oss << mins << " min";
            if (secs > 0) oss << " " << secs << " sec";
        } else {
            oss << secs << " sec";
        }
        return oss.str();
    }
};

CTransportationPlannerCommandLine::CTransportationPlannerCommandLine(
    std::shared_ptr<CDataSource>             cmdsrc,
    std::shared_ptr<CDataSink>               outsink,
    std::shared_ptr<CDataSink>               errsink,
    std::shared_ptr<CDataFactory>            results,
    std::shared_ptr<CTransportationPlanner>  planner)
    : DImplementation(std::make_unique<SImplementation>()) {
    DImplementation->CmdSrc  = cmdsrc;
    DImplementation->OutSink = outsink;
    DImplementation->ErrSink = errsink;
    DImplementation->Results = results;
    DImplementation->Planner = planner;
}

CTransportationPlannerCommandLine::~CTransportationPlannerCommandLine() = default;

bool CTransportationPlannerCommandLine::ProcessCommands() {
    auto &impl = *DImplementation;

    while (true) {
        impl.Write("> ");

        std::string line = impl.ReadLine();

        // Check if source is done (empty line after EOF)
        {
            bool anyData = false;
            for (char c : line) { if (c != ' ' && c != '\t') { anyData = true; break; } }
            if (!anyData && line.empty()) {
                // Try to see if there's more data
                char c;
                if (!impl.CmdSrc->Get(c)) break;
                // Prepend this char and re-read — can't easily un-get,
                // so just treat single-char line
                line += c;
                std::string rest = impl.ReadLine();
                line += rest;
            }
        }

        auto tokens = SImplementation::Tokenize(line);
        if (tokens.empty()) continue;

        const std::string &cmd = tokens[0];

        if (cmd == "exit") {
            break;

        } else if (cmd == "help") {
            impl.Write("------------------------------------------------------------------------\n");
            impl.Write("help     Display this help menu\n");
            impl.Write("exit     Exit the program\n");
            impl.Write("count    Output the number of nodes in the map\n");
            impl.Write("node     Syntax \"node [0, count)\" \n");
            impl.Write("         Will output node ID and Lat/Lon for node\n");
            impl.Write("fastest  Syntax \"fastest start end\" \n");
            impl.Write("         Calculates the time for fastest path from start to end\n");
            impl.Write("shortest Syntax \"shortest start end\" \n");
            impl.Write("         Calculates the distance for the shortest path from start to end\n");
            impl.Write("save     Saves the last calculated path to file\n");
            impl.Write("print    Prints the steps for the last calculated path\n");

        } else if (cmd == "count") {
            impl.Write(std::to_string(impl.Planner->NodeCount()) + " nodes\n");

        } else if (cmd == "node") {
            if (tokens.size() < 2) {
                impl.WriteErr("Invalid node command, see help.\n");
                continue;
            }
            std::size_t idx;
            try {
                long long v = std::stoll(tokens[1]);
                if (v < 0) throw std::out_of_range("negative");
                idx = static_cast<std::size_t>(v);
            } catch (...) {
                impl.WriteErr("Invalid node parameter, see help.\n");
                continue;
            }
            if (idx >= impl.Planner->NodeCount()) {
                impl.WriteErr("Invalid node parameter, see help.\n");
                continue;
            }
            auto node = impl.Planner->SortedNodeByIndex(idx);
            std::string dms = SGeographicUtils::ConvertLLToDMS(node->Location());
            impl.Write("Node " + std::to_string(idx) + ": id = " +
                       std::to_string(node->ID()) + " is at " + dms + "\n");

        } else if (cmd == "shortest") {
            if (tokens.size() < 3) {
                impl.WriteErr("Invalid shortest command, see help.\n");
                continue;
            }
            CTransportationPlanner::TNodeID src, dest;
            try {
                src  = static_cast<CTransportationPlanner::TNodeID>(std::stoull(tokens[1]));
                dest = static_cast<CTransportationPlanner::TNodeID>(std::stoull(tokens[2]));
            } catch (...) {
                impl.WriteErr("Invalid shortest parameter, see help.\n");
                continue;
            }
            std::vector<CTransportationPlanner::TNodeID> sp;
            double dist = impl.Planner->FindShortestPath(src, dest, sp);
            impl.LastSrc  = src;
            impl.LastDest = dest;
            if (dist == CPathRouter::NoPathExists) {
                impl.WriteErr("Unable to find shortest path.\n");
            } else {
                // Use default ostream formatting for distance (e.g. "5.2")
                std::ostringstream oss;
                oss << dist;
                impl.Write("Shortest path is " + oss.str() + " mi.\n");
                impl.HasPath       = true;
                impl.LastWasFastest = false;
                impl.LastDist      = dist;
                impl.ShortPath     = sp;
            }

        } else if (cmd == "fastest") {
            if (tokens.size() < 3) {
                impl.WriteErr("Invalid fastest command, see help.\n");
                continue;
            }
            CTransportationPlanner::TNodeID src, dest;
            try {
                src  = static_cast<CTransportationPlanner::TNodeID>(std::stoull(tokens[1]));
                dest = static_cast<CTransportationPlanner::TNodeID>(std::stoull(tokens[2]));
            } catch (...) {
                impl.WriteErr("Invalid fastest parameter, see help.\n");
                continue;
            }
            std::vector<CTransportationPlanner::TTripStep> fp;
            double time = impl.Planner->FindFastestPath(src, dest, fp);
            impl.LastSrc  = src;
            impl.LastDest = dest;
            if (time == CPathRouter::NoPathExists) {
                impl.WriteErr("Unable to find fastest path.\n");
            } else {
                impl.Write("Fastest path takes " + SImplementation::FormatTime(time) + ".\n");
                impl.HasPath        = true;
                impl.LastWasFastest = true;
                impl.LastTime       = time;
                impl.FastPath       = fp;
            }

        } else if (cmd == "save") {
            if (!impl.HasPath) {
                impl.WriteErr("No valid path to save, see help.\n");
                continue;
            }

            // Build filename using printf %f for time (gives "1.375000")
            char buf[256];
            std::string filename;
            if (impl.LastWasFastest && !impl.FastPath.empty()) {
                snprintf(buf, sizeof(buf), "%llu_%llu_%fhr.csv",
                    (unsigned long long)impl.LastSrc,
                    (unsigned long long)impl.LastDest,
                    impl.LastTime);
                filename = buf;
            } else if (!impl.ShortPath.empty()) {
                snprintf(buf, sizeof(buf), "%llu_%llu_%fmi.csv",
                    (unsigned long long)impl.LastSrc,
                    (unsigned long long)impl.LastDest,
                    impl.LastDist);
                filename = buf;
            } else {
                impl.WriteErr("No valid path to save, see help.\n");
                continue;
            }

            auto sink = impl.Results->CreateSink(filename);
            if (!sink) {
                impl.WriteErr("Failed to create save file.\n");
                continue;
            }

            auto WriteTo = [&](const std::string &s) {
                for (char c : s) sink->Put(c);
            };

            if (impl.LastWasFastest) {
                WriteTo("mode,node_id\n");
                for (std::size_t k = 0; k < impl.FastPath.size(); k++) {
                    auto &step = impl.FastPath[k];
                    std::string modeName;
                    switch (step.first) {
                        case CTransportationPlanner::ETransportationMode::Walk: modeName = "Walk"; break;
                        case CTransportationPlanner::ETransportationMode::Bike: modeName = "Bike"; break;
                        case CTransportationPlanner::ETransportationMode::Bus:  modeName = "Bus";  break;
                    }
                    WriteTo(modeName + "," + std::to_string(step.second));
                    if (k + 1 < impl.FastPath.size()) WriteTo("\n");
                }
            } else {
                WriteTo("node_id\n");
                for (std::size_t k = 0; k < impl.ShortPath.size(); k++) {
                    WriteTo(std::to_string(impl.ShortPath[k]));
                    if (k + 1 < impl.ShortPath.size()) WriteTo("\n");
                }
            }

            impl.Write("Path saved to <results>/" + filename + "\n");

        } else if (cmd == "print") {
            if (!impl.HasPath || !impl.LastWasFastest) {
                impl.WriteErr("No valid path to print, see help.\n");
                continue;
            }
            std::vector<std::string> desc;
            if (impl.Planner->GetPathDescription(impl.FastPath, desc)) {
                for (auto &d : desc)
                    impl.Write(d + "\n");
            } else {
                impl.WriteErr("Failed to get path description.\n");
            }

        } else {
            impl.WriteErr("Unknown command \"" + cmd + "\" type help for help.\n");
        }
    }
    return true;
}