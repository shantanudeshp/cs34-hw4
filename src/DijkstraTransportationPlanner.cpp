#include "DijkstraTransportationPlanner.h"
#include "DijkstraPathRouter.h"
#include "BusSystemIndexer.h"
#include "GeographicUtils.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <chrono>

struct CDijkstraTransportationPlanner::SImplementation {
    std::shared_ptr<CTransportationPlanner::SConfiguration> Config;
    std::shared_ptr<CBusSystemIndexer> BusIndexer;

    std::vector<std::shared_ptr<CStreetMap::SNode>> SortedNodes;
    std::unordered_map<CStreetMap::TNodeID, std::size_t> NodeIndex;

    std::size_t N = 0;

    // ShortRouter: vertex i = SortedNodes[i], edges respect oneway, weight = distance
    CDijkstraPathRouter ShortRouter;

    // FastRouter: 3*N vertices
    //   walk layer : [0,   N)  — bidir, walk speed, ignores oneway
    //   bike layer : [N,  2N)  — respects oneway, bike speed, no bicycle=no
    //   bus  layer : [2N, 3N)  — edges only between consecutive route stops,
    //                            weight = road dist / speed_limit + BusStopTime per stop
    //
    // Mode transitions (at same physical node idx):
    //   walk <-> bike : free, every node
    //   walk -> bus   : free (boarding cost baked into bus edges), only at stops
    //   bus  -> walk  : free, only at stops
    CDijkstraPathRouter FastRouter;

    void Build() {
        auto streetMap  = Config->StreetMap();
        auto busSystem  = Config->BusSystem();
        BusIndexer = std::make_shared<CBusSystemIndexer>(busSystem);

        // Collect and sort nodes by ID
        for (std::size_t i = 0; i < streetMap->NodeCount(); i++)
            SortedNodes.push_back(streetMap->NodeByIndex(i));
        std::sort(SortedNodes.begin(), SortedNodes.end(),
            [](const auto &a, const auto &b){ return a->ID() < b->ID(); });

        N = SortedNodes.size();
        for (std::size_t i = 0; i < N; i++)
            NodeIndex[SortedNodes[i]->ID()] = i;

        // Add vertices
        for (std::size_t i = 0; i < N; i++)
            ShortRouter.AddVertex(SortedNodes[i]->ID());

        for (std::size_t i = 0; i < 3 * N; i++) {
            std::size_t ni = (i < N) ? i : (i < 2*N) ? i - N : i - 2*N;
            FastRouter.AddVertex(SortedNodes[ni]->ID());
        }

        double walkSpeed    = Config->WalkSpeed();
        double bikeSpeed    = Config->BikeSpeed();
        double defaultSpeed = Config->DefaultSpeedLimit();
        double busStopHrs   = Config->BusStopTime() / 3600.0;

        // Free walk <-> bike at every node
        for (std::size_t i = 0; i < N; i++)
            FastRouter.AddEdge(i, N + i, 0.0, true);

        // Free walk <-> bus at bus stop nodes only (boarding cost is in bus edges)
        for (std::size_t s = 0; s < busSystem->StopCount(); s++) {
            auto stop = busSystem->StopByIndex(s);
            auto it   = NodeIndex.find(stop->NodeID());
            if (it == NodeIndex.end()) continue;
            std::size_t idx = it->second;
            FastRouter.AddEdge(idx,       2*N + idx, 0.0, false); // walk -> bus (board)
            FastRouter.AddEdge(2*N + idx, idx,       0.0, false); // bus  -> walk (alight)
        }

        // Walk and bike edges from street map ways
        for (std::size_t w = 0; w < streetMap->WayCount(); w++) {
            auto way = streetMap->WayByIndex(w);
            if (!way || way->NodeCount() < 2) continue;

            bool oneway = way->HasAttribute("oneway") && way->GetAttribute("oneway") == "yes";
            bool noBike = way->HasAttribute("bicycle") && way->GetAttribute("bicycle") == "no";

            double speedLimit = defaultSpeed;
            if (way->HasAttribute("maxspeed")) {
                try { speedLimit = std::stod(way->GetAttribute("maxspeed")); }
                catch (...) { speedLimit = defaultSpeed; }
            }

            for (std::size_t i = 0; i + 1 < way->NodeCount(); i++) {
                auto n1id  = way->GetNodeID(i);
                auto n2id  = way->GetNodeID(i + 1);
                auto node1 = streetMap->NodeByID(n1id);
                auto node2 = streetMap->NodeByID(n2id);
                if (!node1 || !node2) continue;

                auto it1 = NodeIndex.find(n1id);
                auto it2 = NodeIndex.find(n2id);
                if (it1 == NodeIndex.end() || it2 == NodeIndex.end()) continue;

                std::size_t idx1 = it1->second;
                std::size_t idx2 = it2->second;

                double dist = SGeographicUtils::HaversineDistanceInMiles(
                    node1->Location(), node2->Location());

                // Shortest router: respect oneway
                ShortRouter.AddEdge(idx1, idx2, dist, !oneway);

                // Walk layer: always bidir
                FastRouter.AddEdge(idx1, idx2, dist / walkSpeed, true);

                // Bike layer: respect oneway, skip bicycle=no
                if (!noBike)
                    FastRouter.AddEdge(N + idx1, N + idx2, dist / bikeSpeed, !oneway);
            }
        }

        // Bus layer: connect consecutive stops on each route.
        // For each pair of consecutive stops, find the shortest road path between them
        // (using a temporary distance-based router that respects oneway).
        // Edge weight = road_distance / speed_limit + BusStopTime (charged at the SOURCE stop).
        //
        // We reuse ShortRouter (already built above) to find road distances between stops.
        // But ShortRouter isn't precomputed yet, so we can call FindShortestPath now.

        // Build a speed-limit-aware router for bus travel times between consecutive stops
        // We need per-way speed limits, so we compute bus travel time directly.
        // Strategy: build a temporary router with time weights for bus travel.
        CDijkstraPathRouter BusRoadRouter;
        for (std::size_t i = 0; i < N; i++)
            BusRoadRouter.AddVertex(SortedNodes[i]->ID());

        for (std::size_t w = 0; w < streetMap->WayCount(); w++) {
            auto way = streetMap->WayByIndex(w);
            if (!way || way->NodeCount() < 2) continue;

            bool oneway = way->HasAttribute("oneway") && way->GetAttribute("oneway") == "yes";
            double speedLimit = defaultSpeed;
            if (way->HasAttribute("maxspeed")) {
                try { speedLimit = std::stod(way->GetAttribute("maxspeed")); }
                catch (...) { speedLimit = defaultSpeed; }
            }

            for (std::size_t i = 0; i + 1 < way->NodeCount(); i++) {
                auto n1id  = way->GetNodeID(i);
                auto n2id  = way->GetNodeID(i + 1);
                auto node1 = streetMap->NodeByID(n1id);
                auto node2 = streetMap->NodeByID(n2id);
                if (!node1 || !node2) continue;

                auto it1 = NodeIndex.find(n1id);
                auto it2 = NodeIndex.find(n2id);
                if (it1 == NodeIndex.end() || it2 == NodeIndex.end()) continue;

                double dist = SGeographicUtils::HaversineDistanceInMiles(
                    node1->Location(), node2->Location());

                BusRoadRouter.AddEdge(it1->second, it2->second, dist / speedLimit, !oneway);
            }
        }

        // For each route, add bus layer edges between consecutive stops.
        // Weight = road_travel_time + BusStopTime (at the source/boarding stop).
        for (std::size_t r = 0; r < busSystem->RouteCount(); r++) {
            auto route = busSystem->RouteByIndex(r);
            for (std::size_t s = 0; s + 1 < route->StopCount(); s++) {
                auto srcStop  = busSystem->StopByID(route->GetStopID(s));
                auto destStop = busSystem->StopByID(route->GetStopID(s + 1));
                if (!srcStop || !destStop) continue;

                auto it1 = NodeIndex.find(srcStop->NodeID());
                auto it2 = NodeIndex.find(destStop->NodeID());
                if (it1 == NodeIndex.end() || it2 == NodeIndex.end()) continue;

                std::size_t srcIdx  = it1->second;
                std::size_t destIdx = it2->second;

                std::vector<CPathRouter::TVertexID> dummy;
                double roadTime = BusRoadRouter.FindShortestPath(srcIdx, destIdx, dummy);
                if (roadTime == CPathRouter::NoPathExists) continue;

                // Add BusStopTime at the source stop (charged per stop visited)
                double edgeWeight = roadTime + busStopHrs;

                // Bus layer: directed (bus follows route direction)
                FastRouter.AddEdge(2*N + srcIdx, 2*N + destIdx, edgeWeight, false);
            }
        }

        // Precompute
        auto deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(Config->PrecomputeTime());
        ShortRouter.Precompute(deadline);
        FastRouter.Precompute(deadline);
    }
};

CDijkstraTransportationPlanner::CDijkstraTransportationPlanner(
    std::shared_ptr<SConfiguration> config)
    : DImplementation(std::make_unique<SImplementation>()) {
    DImplementation->Config = config;
    DImplementation->Build();
}

CDijkstraTransportationPlanner::~CDijkstraTransportationPlanner() = default;

std::size_t CDijkstraTransportationPlanner::NodeCount() const noexcept {
    return DImplementation->SortedNodes.size();
}

std::shared_ptr<CStreetMap::SNode>
CDijkstraTransportationPlanner::SortedNodeByIndex(std::size_t index) const noexcept {
    if (index >= DImplementation->SortedNodes.size()) return nullptr;
    return DImplementation->SortedNodes[index];
}

double CDijkstraTransportationPlanner::FindShortestPath(
    TNodeID src, TNodeID dest, std::vector<TNodeID> &path) {
    path.clear();
    auto &impl = *DImplementation;

    auto it1 = impl.NodeIndex.find(src);
    auto it2 = impl.NodeIndex.find(dest);
    if (it1 == impl.NodeIndex.end() || it2 == impl.NodeIndex.end())
        return CPathRouter::NoPathExists;

    std::vector<CPathRouter::TVertexID> vpath;
    double dist = impl.ShortRouter.FindShortestPath(it1->second, it2->second, vpath);
    if (dist == CPathRouter::NoPathExists) return CPathRouter::NoPathExists;

    for (auto vid : vpath)
        path.push_back(impl.SortedNodes[vid]->ID());
    return dist;
}

double CDijkstraTransportationPlanner::FindFastestPath(
    TNodeID src, TNodeID dest, std::vector<TTripStep> &path) {
    path.clear();
    auto &impl = *DImplementation;
    std::size_t N = impl.N;

    auto it1 = impl.NodeIndex.find(src);
    auto it2 = impl.NodeIndex.find(dest);
    if (it1 == impl.NodeIndex.end() || it2 == impl.NodeIndex.end())
        return CPathRouter::NoPathExists;

    std::size_t srcIdx  = it1->second;
    std::size_t destIdx = it2->second;

    std::vector<CPathRouter::TVertexID> vpath;
    double best = impl.FastRouter.FindShortestPath(srcIdx, destIdx, vpath);
    if (best == CPathRouter::NoPathExists) return CPathRouter::NoPathExists;

    // Reconstruct path with modes.
    // Same-physical-node transitions:
    //   walk->bike : update in place (free switch)
    //   bike->walk : keep bike, drop walk (free switch back to walk layer)
    //   walk->bus  : keep Walk,x (is boarding stop), drop Bus,x duplicate
    //   bus->walk  : keep Bus,x (is alighting stop), drop Walk,x duplicate
    for (auto vid : vpath) {
        ETransportationMode mode;
        CStreetMap::TNodeID nodeId;

        if (vid < N) {
            mode   = ETransportationMode::Walk;
            nodeId = impl.SortedNodes[vid]->ID();
        } else if (vid < 2*N) {
            mode   = ETransportationMode::Bike;
            nodeId = impl.SortedNodes[vid - N]->ID();
        } else {
            mode   = ETransportationMode::Bus;
            nodeId = impl.SortedNodes[vid - 2*N]->ID();
        }

        if (!path.empty() && path.back().second == nodeId) {
            auto prevMode = path.back().first;
            if (prevMode == mode) {
                continue; // exact duplicate
            } else if (prevMode == ETransportationMode::Walk && mode == ETransportationMode::Bike) {
                path.back().first = mode; // walk->bike: update in place
            } else if (prevMode == ETransportationMode::Bike && mode == ETransportationMode::Walk) {
                continue; // bike->walk: keep bike, drop walk
            } else if (prevMode == ETransportationMode::Walk && mode == ETransportationMode::Bus) {
                continue; // walk->bus: Walk,x is boarding stop, drop Bus,x
            } else if (prevMode == ETransportationMode::Bus && mode == ETransportationMode::Walk) {
                continue; // bus->walk: Bus,x is alighting stop, drop Walk,x
            } else {
                path.push_back({mode, nodeId});
            }
        } else {
            path.push_back({mode, nodeId});
        }
    }
    return best;
}

bool CDijkstraTransportationPlanner::GetPathDescription(
    const std::vector<TTripStep> &path, std::vector<std::string> &desc) const {
    desc.clear();
    if (path.empty()) return false;

    auto &impl     = *DImplementation;
    auto streetMap = impl.Config->StreetMap();

    auto getNode = [&](CStreetMap::TNodeID nid) {
        return streetMap->NodeByID(nid);
    };

    auto getWayName = [&](CStreetMap::TNodeID n1, CStreetMap::TNodeID n2) -> std::string {
        for (std::size_t w = 0; w < streetMap->WayCount(); w++) {
            auto way = streetMap->WayByIndex(w);
            for (std::size_t i = 0; i + 1 < way->NodeCount(); i++) {
                auto a = way->GetNodeID(i);
                auto b = way->GetNodeID(i + 1);
                if ((a == n1 && b == n2) || (a == n2 && b == n1)) {
                    if (way->HasAttribute("name")) return way->GetAttribute("name");
                    return "";
                }
            }
        }
        return "";
    };

    // Start
    auto startNode = getNode(path[0].second);
    if (!startNode) return false;
    desc.push_back("Start at " + SGeographicUtils::ConvertLLToDMS(startNode->Location()));

    std::size_t i = 0;
    while (i + 1 < path.size()) {
        auto mode = path[i].first;

        // ── Bus segment ──────────────────────────────────────────────────────
        if (mode == ETransportationMode::Bus) {
            std::size_t j = i + 1;
            while (j < path.size() && path[j].first == ETransportationMode::Bus) j++;

            // Boarding stop: check if the preceding Walk step is at a bus stop
            CStreetMap::TNodeID boardingNode = path[i].second;
            if (i > 0 && path[i-1].first == ETransportationMode::Walk) {
                auto prevStop = impl.BusIndexer->StopByNodeID(path[i-1].second);
                if (prevStop) boardingNode = path[i-1].second;
            }
            auto srcStop  = impl.BusIndexer->StopByNodeID(boardingNode);
            auto destStop = impl.BusIndexer->StopByNodeID(path[j - 1].second);

            // Pick best route: goes furthest, tie-break by earliest name
            std::string routeName = "?";
            if (srcStop && destStop) {
                std::shared_ptr<CBusSystemIndexer::SRoute> bestRoute;
                std::size_t bestFurthest = 0;

                for (std::size_t r = 0; r < impl.BusIndexer->RouteCount(); r++) {
                    auto route    = impl.BusIndexer->SortedRouteByIndex(r);
                    bool foundSrc = false;
                    std::size_t furthest = 0;

                    for (std::size_t s = 0; s < route->StopCount(); s++) {
                        if (route->GetStopID(s) == srcStop->ID()) {
                            foundSrc = true;
                            furthest = 0;
                        } else if (foundSrc) {
                            furthest++;
                            if (route->GetStopID(s) == destStop->ID()) {
                                if (!bestRoute || furthest > bestFurthest ||
                                    (furthest == bestFurthest && route->Name() < bestRoute->Name())) {
                                    bestFurthest = furthest;
                                    bestRoute    = route;
                                }
                                break;
                            }
                        }
                    }
                }
                if (bestRoute) routeName = bestRoute->Name();
            }

            std::string stopSrc  = srcStop  ? std::to_string(srcStop->ID())  : "?";
            std::string stopDest = destStop ? std::to_string(destStop->ID()) : "?";
            desc.push_back("Take Bus " + routeName +
                           " from stop " + stopSrc + " to stop " + stopDest);
            i = j;
            continue;
        }

        // ── Walk or Bike segment ─────────────────────────────────────────────
        std::string modeStr = (mode == ETransportationMode::Walk) ? "Walk" : "Bike";

        std::size_t j = i + 1;
        while (j < path.size() && path[j].first == mode) j++;

        // If previous segment was Bus and ended at a different node than path[i],
        // we need to treat the bus alighting node as the start of this walk segment.
        CStreetMap::TNodeID walkStartNode = path[i].second;
        if (i > 0 && path[i-1].first == ETransportationMode::Bus &&
            path[i-1].second != path[i].second) {
            walkStartNode = path[i-1].second;
        }

        // Group into sub-segments by street name
        // Build a temporary node list including the alighting node if needed
        std::vector<CStreetMap::TNodeID> segNodes;
        if (walkStartNode != path[i].second)
            segNodes.push_back(walkStartNode);
        for (std::size_t k = i; k < j; k++)
            segNodes.push_back(path[k].second);

        std::size_t segStart = 0;
        while (segStart + 1 < segNodes.size()) {
            std::string segName = getWayName(segNodes[segStart], segNodes[segStart + 1]);
            std::size_t segEnd  = segStart + 1;

            while (segEnd + 1 < segNodes.size()) {
                std::string nm = getWayName(segNodes[segEnd], segNodes[segEnd + 1]);
                if (nm != segName) break;
                segEnd++;
            }

            double dist = 0.0;
            for (std::size_t k = segStart; k < segEnd; k++) {
                auto na = getNode(segNodes[k]);
                auto nb = getNode(segNodes[k + 1]);
                if (na && nb)
                    dist += SGeographicUtils::HaversineDistanceInMiles(
                        na->Location(), nb->Location());
            }

            auto firstN = getNode(segNodes[segStart]);
            auto lastN  = getNode(segNodes[segEnd]);
            std::string dirStr;
            if (firstN && lastN)
                dirStr = SGeographicUtils::BearingToDirection(
                    SGeographicUtils::CalculateBearing(firstN->Location(), lastN->Location()));

            std::ostringstream oss;
            oss << std::fixed;
            oss.precision(1);
            oss << dist;

            if (!segName.empty()) {
                desc.push_back(modeStr + " " + dirStr + " along " + segName +
                               " for " + oss.str() + " mi");
            } else {
                // Find next known street name ahead
                std::string nextName;
                for (std::size_t k = segEnd; k + 1 < segNodes.size(); k++) {
                    nextName = getWayName(segNodes[k], segNodes[k + 1]);
                    if (!nextName.empty()) break;
                }
                // Also look in remaining path
                if (nextName.empty()) {
                    for (std::size_t k = j; k + 1 < path.size(); k++) {
                        nextName = getWayName(path[k].second, path[k + 1].second);
                        if (!nextName.empty()) break;
                    }
                }
                if (nextName.empty()) nextName = "End";
                desc.push_back(modeStr + " " + dirStr + " toward " + nextName +
                               " for " + oss.str() + " mi");
            }
            segStart = segEnd;
        }
        i = j;
    }

    // End
    auto endNode = getNode(path.back().second);
    if (!endNode) return false;
    desc.push_back("End at " + SGeographicUtils::ConvertLLToDMS(endNode->Location()));

    return true;
}