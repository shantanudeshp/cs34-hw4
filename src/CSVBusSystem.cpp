#include "CSVBusSystem.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

struct CCSVBusSystem::SImplementation {

    // concrete stop and route types live here to keep them out of the header
    struct SStop : public CBusSystem::SStop {
        TStopID DID;
        CStreetMap::TNodeID DNodeID;

        TStopID ID() const noexcept override { return DID; }
        CStreetMap::TNodeID NodeID() const noexcept override { return DNodeID; }
    };

    struct SRoute : public CBusSystem::SRoute {
        std::string DName;
        std::vector<TStopID> DStopIDs;

        std::string Name() const noexcept override { return DName; }
        std::size_t StopCount() const noexcept override { return DStopIDs.size(); }

        TStopID GetStopID(std::size_t index) const noexcept override {
            if(index < DStopIDs.size()){
                return DStopIDs[index];
            }
            return CBusSystem::InvalidStopID;
        }
    };

    // stops in insertion order, plus a map for O(1) id lookup
    std::vector<std::shared_ptr<SStop>> DStops;
    std::unordered_map<TStopID, std::size_t> DStopByID;

    // routes in insertion order, plus a map for O(1) name lookup
    std::vector<std::shared_ptr<SRoute>> DRoutes;
    std::unordered_map<std::string, std::size_t> DRouteByName;
};

CCSVBusSystem::CCSVBusSystem(std::shared_ptr<CDSVReader> stopsrc, std::shared_ptr<CDSVReader> routesrc)
    : DImplementation(std::make_unique<SImplementation>()) {

    std::vector<std::string> Row;

    // skip header row then read each stop
    stopsrc->ReadRow(Row);
    while(!stopsrc->End()){
        if(stopsrc->ReadRow(Row) && Row.size() >= 2){
            auto Stop = std::make_shared<SImplementation::SStop>();
            Stop->DID = std::stoull(Row[0]);
            Stop->DNodeID = std::stoull(Row[1]);
            DImplementation->DStopByID[Stop->DID] = DImplementation->DStops.size();
            DImplementation->DStops.push_back(Stop);
        }
    }

    // skip header row, then group stops by route name preserving order
    routesrc->ReadRow(Row);
    while(!routesrc->End()){
        if(routesrc->ReadRow(Row) && Row.size() >= 2){
            std::string RouteName = Row[0];
            TStopID StopID = std::stoull(Row[1]);

            auto It = DImplementation->DRouteByName.find(RouteName);
            if(It == DImplementation->DRouteByName.end()){
                auto Route = std::make_shared<SImplementation::SRoute>();
                Route->DName = RouteName;
                Route->DStopIDs.push_back(StopID);
                DImplementation->DRouteByName[RouteName] = DImplementation->DRoutes.size();
                DImplementation->DRoutes.push_back(Route);
            } else {
                DImplementation->DRoutes[It->second]->DStopIDs.push_back(StopID);
            }
        }
    }
}

CCSVBusSystem::~CCSVBusSystem() = default;

std::size_t CCSVBusSystem::StopCount() const noexcept {
    return DImplementation->DStops.size();
}

std::size_t CCSVBusSystem::RouteCount() const noexcept {
    return DImplementation->DRoutes.size();
}

std::shared_ptr<CBusSystem::SStop> CCSVBusSystem::StopByIndex(std::size_t index) const noexcept {
    if(index < DImplementation->DStops.size()){
        return DImplementation->DStops[index];
    }
    return nullptr;
}

std::shared_ptr<CBusSystem::SStop> CCSVBusSystem::StopByID(TStopID id) const noexcept {
    auto It = DImplementation->DStopByID.find(id);
    if(It != DImplementation->DStopByID.end()){
        return DImplementation->DStops[It->second];
    }
    return nullptr;
}

std::shared_ptr<CBusSystem::SRoute> CCSVBusSystem::RouteByIndex(std::size_t index) const noexcept {
    if(index < DImplementation->DRoutes.size()){
        return DImplementation->DRoutes[index];
    }
    return nullptr;
}

std::shared_ptr<CBusSystem::SRoute> CCSVBusSystem::RouteByName(const std::string &name) const noexcept {
    auto It = DImplementation->DRouteByName.find(name);
    if(It != DImplementation->DRouteByName.end()){
        return DImplementation->DRoutes[It->second];
    }
    return nullptr;
}
