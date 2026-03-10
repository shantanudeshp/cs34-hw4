#include "OpenStreetMap.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

struct COpenStreetMap::SImplementation {

    struct SNodeImpl : public CStreetMap::SNode {
        TNodeID DID = CStreetMap::InvalidNodeID;
        SLocation DLocation{0.0, 0.0};

        // Preserve insertion order for keys, but allow fast lookup.
        std::vector<TAttribute> DAttributes;
        std::unordered_map<std::string, std::string> DAttributeByKey;

        TNodeID ID() const noexcept override { return DID; }
        SLocation Location() const noexcept override { return DLocation; }

        std::size_t AttributeCount() const noexcept override { return DAttributes.size(); }

        std::string GetAttributeKey(std::size_t index) const noexcept override {
            if(index < DAttributes.size()){
                return std::get<0>(DAttributes[index]);
            }
            return std::string();
        }

        bool HasAttribute(const std::string &key) const noexcept override {
            return DAttributeByKey.find(key) != DAttributeByKey.end();
        }

        std::string GetAttribute(const std::string &key) const noexcept override {
            auto It = DAttributeByKey.find(key);
            if(It != DAttributeByKey.end()){
                return It->second;
            }
            return std::string();
        }

        void SetAttribute(const std::string &key, const std::string &val){
            if(key.empty()){
                return;
            }
            auto It = DAttributeByKey.find(key);
            if(It == DAttributeByKey.end()){
                DAttributes.push_back(std::make_pair(key, val));
                DAttributeByKey[key] = val;
            } else {
                It->second = val;
                for(auto &Attr : DAttributes){
                    if(std::get<0>(Attr) == key){
                        Attr = std::make_pair(key, val);
                        break;
                    }
                }
            }
        }
    };

    struct SWayImpl : public CStreetMap::SWay {
        TWayID DID = CStreetMap::InvalidWayID;
        std::vector<TNodeID> DNodeIDs;

        std::vector<TAttribute> DAttributes;
        std::unordered_map<std::string, std::string> DAttributeByKey;

        TWayID ID() const noexcept override { return DID; }

        std::size_t NodeCount() const noexcept override { return DNodeIDs.size(); }

        TNodeID GetNodeID(std::size_t index) const noexcept override {
            if(index < DNodeIDs.size()){
                return DNodeIDs[index];
            }
            return CStreetMap::InvalidNodeID;
        }

        std::size_t AttributeCount() const noexcept override { return DAttributes.size(); }

        std::string GetAttributeKey(std::size_t index) const noexcept override {
            if(index < DAttributes.size()){
                return std::get<0>(DAttributes[index]);
            }
            return std::string();
        }

        bool HasAttribute(const std::string &key) const noexcept override {
            return DAttributeByKey.find(key) != DAttributeByKey.end();
        }

        std::string GetAttribute(const std::string &key) const noexcept override {
            auto It = DAttributeByKey.find(key);
            if(It != DAttributeByKey.end()){
                return It->second;
            }
            return std::string();
        }

        void SetAttribute(const std::string &key, const std::string &val){
            if(key.empty()){
                return;
            }
            auto It = DAttributeByKey.find(key);
            if(It == DAttributeByKey.end()){
                DAttributes.push_back(std::make_pair(key, val));
                DAttributeByKey[key] = val;
            } else {
                It->second = val;
                for(auto &Attr : DAttributes){
                    if(std::get<0>(Attr) == key){
                        Attr = std::make_pair(key, val);
                        break;
                    }
                }
            }
        }
    };

    std::vector<std::shared_ptr<SNodeImpl>> DNodes;
    std::unordered_map<TNodeID, std::size_t> DNodeByID;

    std::vector<std::shared_ptr<SWayImpl>> DWays;
    std::unordered_map<TWayID, std::size_t> DWayByID;
};

COpenStreetMap::COpenStreetMap(std::shared_ptr<CXMLReader> src)
    : DImplementation(std::make_unique<SImplementation>()) {

    if(!src){
        return;
    }

    SXMLEntity Ent;

    while(src->ReadEntity(Ent, true)){
        // ----- node -----
        if((Ent.DType == SXMLEntity::EType::StartElement || Ent.DType == SXMLEntity::EType::CompleteElement) && Ent.DNameData == "node"){
            auto Node = std::make_shared<SImplementation::SNodeImpl>();

            if(Ent.AttributeExists("id")){
                try{ Node->DID = std::stoull(Ent.AttributeValue("id")); }
                catch(...){ Node->DID = CStreetMap::InvalidNodeID; }
            }
            if(Ent.AttributeExists("lat") && Ent.AttributeExists("lon")){
                try{
                    double Lat = std::stod(Ent.AttributeValue("lat"));
                    double Lon = std::stod(Ent.AttributeValue("lon"));
                    Node->DLocation = CStreetMap::SLocation(Lat, Lon);
                }
                catch(...){}
            }

            // Non-self-closing nodes can have <tag .../> children.
            if(Ent.DType == SXMLEntity::EType::StartElement){
                SXMLEntity Child;
                while(src->ReadEntity(Child, true)){
                    if(Child.DType == SXMLEntity::EType::EndElement && Child.DNameData == "node"){
                        break;
                    }

                    if((Child.DType == SXMLEntity::EType::CompleteElement || Child.DType == SXMLEntity::EType::StartElement) && Child.DNameData == "tag"){
                        Node->SetAttribute(Child.AttributeValue("k"), Child.AttributeValue("v"));

                        // Defensive: consume until </tag> if it wasn't a complete element
                        if(Child.DType == SXMLEntity::EType::StartElement){
                            SXMLEntity Skip;
                            while(src->ReadEntity(Skip, true)){
                                if(Skip.DType == SXMLEntity::EType::EndElement && Skip.DNameData == "tag"){
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if(Node->DID != CStreetMap::InvalidNodeID){
                DImplementation->DNodeByID[Node->DID] = DImplementation->DNodes.size();
            }
            DImplementation->DNodes.push_back(Node);
        }

        // ----- way -----
        else if((Ent.DType == SXMLEntity::EType::StartElement || Ent.DType == SXMLEntity::EType::CompleteElement) && Ent.DNameData == "way"){
            auto Way = std::make_shared<SImplementation::SWayImpl>();

            if(Ent.AttributeExists("id")){
                try{ Way->DID = std::stoull(Ent.AttributeValue("id")); }
                catch(...){ Way->DID = CStreetMap::InvalidWayID; }
            }

            // Non-self-closing ways can have <nd ref="..."/> and <tag .../> children.
            if(Ent.DType == SXMLEntity::EType::StartElement){
                SXMLEntity Child;
                while(src->ReadEntity(Child, true)){
                    if(Child.DType == SXMLEntity::EType::EndElement && Child.DNameData == "way"){
                        break;
                    }

                    if((Child.DType == SXMLEntity::EType::CompleteElement || Child.DType == SXMLEntity::EType::StartElement) && Child.DNameData == "nd"){
                        if(Child.AttributeExists("ref")){
                            Way->DNodeIDs.push_back(std::stoull(Child.AttributeValue("ref")));
                        }

                        if(Child.DType == SXMLEntity::EType::StartElement){
                            SXMLEntity Skip;
                            while(src->ReadEntity(Skip, true)){
                                if(Skip.DType == SXMLEntity::EType::EndElement && Skip.DNameData == "nd"){
                                    break;
                                }
                            }
                        }
                    }
                    else if((Child.DType == SXMLEntity::EType::CompleteElement || Child.DType == SXMLEntity::EType::StartElement) && Child.DNameData == "tag"){
                        Way->SetAttribute(Child.AttributeValue("k"), Child.AttributeValue("v"));

                        if(Child.DType == SXMLEntity::EType::StartElement){
                            SXMLEntity Skip;
                            while(src->ReadEntity(Skip, true)){
                                if(Skip.DType == SXMLEntity::EType::EndElement && Skip.DNameData == "tag"){
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if(Way->DID != CStreetMap::InvalidWayID && DImplementation->DWayByID.find(Way->DID) == DImplementation->DWayByID.end()){
                DImplementation->DWayByID[Way->DID] = DImplementation->DWays.size();
                DImplementation->DWays.push_back(Way);
            }
        }
    }
}

COpenStreetMap::~COpenStreetMap() = default;

std::size_t COpenStreetMap::NodeCount() const noexcept {
    return DImplementation->DNodes.size();
}

std::size_t COpenStreetMap::WayCount() const noexcept {
    return DImplementation->DWays.size();
}

std::shared_ptr<CStreetMap::SNode> COpenStreetMap::NodeByIndex(std::size_t index) const noexcept {
    if(index < DImplementation->DNodes.size()){
        return DImplementation->DNodes[index];
    }
    return nullptr;
}

std::shared_ptr<CStreetMap::SNode> COpenStreetMap::NodeByID(TNodeID id) const noexcept {
    auto It = DImplementation->DNodeByID.find(id);
    if(It != DImplementation->DNodeByID.end()){
        return DImplementation->DNodes[It->second];
    }
    return nullptr;
}

std::shared_ptr<CStreetMap::SWay> COpenStreetMap::WayByIndex(std::size_t index) const noexcept {
    if(index < DImplementation->DWays.size()){
        return DImplementation->DWays[index];
    }
    return nullptr;
}

std::shared_ptr<CStreetMap::SWay> COpenStreetMap::WayByID(TWayID id) const noexcept {
    auto It = DImplementation->DWayByID.find(id);
    if(It != DImplementation->DWayByID.end()){
        return DImplementation->DWays[It->second];
    }
    return nullptr;
}