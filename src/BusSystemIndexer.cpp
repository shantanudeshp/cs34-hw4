#include "BusSystemIndexer.h"
#include <vector>
#include <algorithm>
#include <unordered_map>

struct CBusSystemIndexer::SImplementation {
    std::vector<std::shared_ptr<SStop>> SortedStops;
    std::vector<std::shared_ptr<SRoute>> SortedRoutes;
    std::unordered_map<TNodeID, std::shared_ptr<SStop>> StopByNode;
};

CBusSystemIndexer::CBusSystemIndexer(std::shared_ptr<CBusSystem> bussystem)
    : DImplementation(std::make_unique<SImplementation>()) {
    for(std::size_t i = 0; i < bussystem->StopCount(); i++)
        DImplementation->SortedStops.push_back(bussystem->StopByIndex(i));
    std::sort(DImplementation->SortedStops.begin(), DImplementation->SortedStops.end(),
        [](const auto &a, const auto &b){ return a->ID() < b->ID(); });
    for(auto &stop : DImplementation->SortedStops)
        DImplementation->StopByNode[stop->NodeID()] = stop;

    for(std::size_t i = 0; i < bussystem->RouteCount(); i++)
        DImplementation->SortedRoutes.push_back(bussystem->RouteByIndex(i));
    std::sort(DImplementation->SortedRoutes.begin(), DImplementation->SortedRoutes.end(),
        [](const auto &a, const auto &b){ return a->Name() < b->Name(); });
}

CBusSystemIndexer::~CBusSystemIndexer() = default;

std::size_t CBusSystemIndexer::StopCount() const noexcept {
    return DImplementation->SortedStops.size();
}

std::size_t CBusSystemIndexer::RouteCount() const noexcept {
    return DImplementation->SortedRoutes.size();
}

std::shared_ptr<CBusSystemIndexer::SStop> CBusSystemIndexer::SortedStopByIndex(std::size_t index) const noexcept {
    if(index >= DImplementation->SortedStops.size()) return nullptr;
    return DImplementation->SortedStops[index];
}

std::shared_ptr<CBusSystemIndexer::SRoute> CBusSystemIndexer::SortedRouteByIndex(std::size_t index) const noexcept {
    if(index >= DImplementation->SortedRoutes.size()) return nullptr;
    return DImplementation->SortedRoutes[index];
}

std::shared_ptr<CBusSystemIndexer::SStop> CBusSystemIndexer::StopByNodeID(TNodeID id) const noexcept {
    auto it = DImplementation->StopByNode.find(id);
    return it != DImplementation->StopByNode.end() ? it->second : nullptr;
}

bool CBusSystemIndexer::RoutesByNodeIDs(TNodeID src, TNodeID dest, std::unordered_set<std::shared_ptr<SRoute>> &routes) const noexcept {
    auto srcStop = StopByNodeID(src);
    auto destStop = StopByNodeID(dest);
    if(!srcStop || !destStop) return false;

    for(auto &route : DImplementation->SortedRoutes){
        bool foundSrc = false;
        for(std::size_t i = 0; i < route->StopCount(); i++){
            if(route->GetStopID(i) == srcStop->ID()) foundSrc = true;
            else if(foundSrc && route->GetStopID(i) == destStop->ID()){
                routes.insert(route);
                break;
            }
        }
    }
    return !routes.empty();
}

bool CBusSystemIndexer::RouteBetweenNodeIDs(TNodeID src, TNodeID dest) const noexcept {
    std::unordered_set<std::shared_ptr<SRoute>> routes;
    return RoutesByNodeIDs(src, dest, routes);
}
