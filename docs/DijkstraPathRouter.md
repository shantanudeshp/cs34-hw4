# CDijkstraPathRouter

Concrete implementation of `CPathRouter` using Dijkstra's algorithm with a min-heap priority queue. Stores the graph as an adjacency list.

## Construction

```cpp
CDijkstraPathRouter router;
```

No arguments. Vertices and edges are added dynamically.

## Adding Vertices

```cpp
auto v0 = router.AddVertex(std::string("start"));
auto v1 = router.AddVertex(42);  // tag can be any type
```

Vertices are assigned IDs sequentially starting at 0. The tag can be any type via `std::any` — used to associate graph vertices with external data (e.g. OSM node IDs).

## Adding Edges

```cpp
router.AddEdge(v0, v1, 2.5);        // directed
router.AddEdge(v0, v1, 2.5, true);  // bidirectional
```

Returns `false` if either vertex ID is invalid or weight is negative.

## Finding Shortest Path

```cpp
std::vector<CPathRouter::TVertexID> path;
double dist = router.FindShortestPath(v0, v2, path);
if(dist == CPathRouter::NoPathExists){
    // no path
}
```

Runs Dijkstra's from `src`. Returns total distance and fills `path` with vertex IDs in order. Returns `NoPathExists` if unreachable.

## Precompute

```cpp
router.Precompute(std::chrono::steady_clock::now() + std::chrono::seconds(30));
```

No-op in this implementation — Dijkstra runs on demand. Always returns `true`.

## Complexity

- `AddVertex`: O(1)
- `AddEdge`: O(1)
- `FindShortestPath`: O((V + E) log V) using a binary min-heap

## Example

```cpp
CDijkstraPathRouter router;
auto A = router.AddVertex(std::string("A"));
auto B = router.AddVertex(std::string("B"));
auto C = router.AddVertex(std::string("C"));
router.AddEdge(A, B, 1.0);
router.AddEdge(A, C, 10.0);
router.AddEdge(B, C, 2.0);

std::vector<CPathRouter::TVertexID> path;
double dist = router.FindShortestPath(A, C, path);
// dist == 3.0, path == {A, B, C}
```
