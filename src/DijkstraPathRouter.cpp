#include "DijkstraPathRouter.h"
#include <vector>
#include <any>
#include <queue>
#include <limits>

struct CDijkstraPathRouter::SImplementation {
    std::vector<std::any> Tags;
    std::vector<std::vector<std::pair<TVertexID, double>>> Adj;
};

CDijkstraPathRouter::CDijkstraPathRouter()
    : DImplementation(std::make_unique<SImplementation>()) {}

CDijkstraPathRouter::~CDijkstraPathRouter() = default;

std::size_t CDijkstraPathRouter::VertexCount() const noexcept {
    return DImplementation->Tags.size();
}

CPathRouter::TVertexID CDijkstraPathRouter::AddVertex(std::any tag) noexcept {
    DImplementation->Tags.push_back(tag);
    DImplementation->Adj.emplace_back();
    return DImplementation->Tags.size() - 1;
}

std::any CDijkstraPathRouter::GetVertexTag(TVertexID id) const noexcept {
    if(id >= DImplementation->Tags.size()) return std::any();
    return DImplementation->Tags[id];
}

bool CDijkstraPathRouter::AddEdge(TVertexID src, TVertexID dest, double weight, bool bidir) noexcept {
    if(src >= VertexCount() || dest >= VertexCount() || weight < 0) return false;
    DImplementation->Adj[src].push_back({dest, weight});
    if(bidir) DImplementation->Adj[dest].push_back({src, weight});
    return true;
}

bool CDijkstraPathRouter::Precompute(std::chrono::steady_clock::time_point) noexcept {
    return true;
}

double CDijkstraPathRouter::FindShortestPath(TVertexID src, TVertexID dest, std::vector<TVertexID> &path) noexcept {
    path.clear();
    std::size_t N = VertexCount();
    if(src >= N || dest >= N) return NoPathExists;

    std::vector<double> Dist(N, NoPathExists);
    std::vector<TVertexID> Prev(N, InvalidVertexID);
    std::priority_queue<std::pair<double,TVertexID>,
                        std::vector<std::pair<double,TVertexID>>,
                        std::greater<>> PQ;
    Dist[src] = 0.0;
    PQ.push({0.0, src});

    while(!PQ.empty()){
        auto [d, u] = PQ.top(); PQ.pop();
        if(d > Dist[u]) continue;
        for(auto &[v, w] : DImplementation->Adj[u]){
            if(Dist[u] + w < Dist[v]){
                Dist[v] = Dist[u] + w;
                Prev[v] = u;
                PQ.push({Dist[v], v});
            }
        }
    }

    if(Dist[dest] == NoPathExists) return NoPathExists;

    for(TVertexID v = dest; v != InvalidVertexID; v = Prev[v])
        path.push_back(v);
    std::reverse(path.begin(), path.end());
    return Dist[dest];
}
