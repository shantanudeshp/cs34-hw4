# CPathRouter

Abstract base class for a weighted directed graph path router. Defines the interface for adding vertices and edges and finding shortest paths.

## Types

```cpp
using TVertexID = std::size_t;
static constexpr TVertexID InvalidVertexID = std::numeric_limits<TVertexID>::max();
static constexpr double NoPathExists = std::numeric_limits<double>::max();
```

## Interface

```cpp
virtual std::size_t VertexCount() const noexcept = 0;
virtual TVertexID AddVertex(std::any tag) noexcept = 0;
virtual std::any GetVertexTag(TVertexID id) const noexcept = 0;
virtual bool AddEdge(TVertexID src, TVertexID dest, double weight, bool bidir = false) noexcept = 0;
virtual bool Precompute(std::chrono::steady_clock::time_point deadline) noexcept = 0;
virtual double FindShortestPath(TVertexID src, TVertexID dest, std::vector<TVertexID> &path) noexcept = 0;
```

- `AddVertex(tag)` — adds a vertex with an arbitrary tag, returns its ID
- `GetVertexTag(id)` — returns the tag for a vertex, or empty `std::any()` if invalid
- `AddEdge(src, dest, weight, bidir)` — adds a directed edge; if `bidir=true`, adds both directions
- `Precompute(deadline)` — optional precomputation up to a time deadline; implementations may no-op
- `FindShortestPath(src, dest, path)` — fills `path` with vertex IDs and returns total distance; returns `NoPathExists` if unreachable

## Example

```cpp
CDijkstraPathRouter router;
auto A = router.AddVertex(std::string("A"));
auto B = router.AddVertex(std::string("B"));
router.AddEdge(A, B, 3.5);

std::vector<CPathRouter::TVertexID> path;
double dist = router.FindShortestPath(A, B, path);
// dist == 3.5, path == {A, B}
```
