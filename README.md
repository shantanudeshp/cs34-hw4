# Project 4 - Dijkstra Transportation Planner

Student: Shantanu (921200468), Ryan Duong (921278843)

## Overview

Implementation of `CDijkstraPathRouter` and `CBusSystemIndexer` as the foundational layer for a Davis, CA navigation system. The path router provides a generic weighted graph with Dijkstra's shortest path algorithm. The bus system indexer wraps `CBusSystem` with sorted access and fast lookup by node ID and route connectivity. Both use the pimpl pattern and build on classes ported from Project 3.

## Build and Run

```
make        # compiles and runs all tests
make clean  # removes all build artifacts
```

## Code References

* [C++ Reference Documentation](http://www.cplusplus.com/reference/) - reference for C++ standard library
* [Dijkstra's Algorithm - Wikipedia](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm) - algorithm reference
* [OpenStreetMap XML Format](https://wiki.openstreetmap.org/wiki/OSM_XML) - OSM data format
* ECS34 guides provided on Canvas

## Generative AI Use

- AI-assisted tab completion (IDE autocomplete) - explicitly permitted per course policy
- Consulted generative AI assistant for implementation guidance and debugging

---

### 1. Adjacency List vs Adjacency Matrix for Dijkstra

**Prompt:** "Should I use an adjacency list or adjacency matrix to store the graph for Dijkstra's algorithm in this project?"

**Original AI Response:**
"Use an adjacency list. An adjacency matrix uses O(V²) memory which is wasteful for sparse graphs like street maps where each node typically has only 2-4 neighbors. An adjacency list uses O(V + E) memory and iterates only over actual neighbors during Dijkstra's relaxation step, making it much faster in practice. Represent it as a `vector<vector<pair<TVertexID, double>>>` where the outer index is the source vertex ID and the inner vector contains (destination, weight) pairs."

**Changes Made:** Stored the graph as `vector<vector<pair<TVertexID, double>>> Adj` inside `SImplementation`. The outer vector is indexed by vertex ID so neighbor iteration is O(degree) rather than O(V).

**Rationale:** Street maps are highly sparse — an adjacency matrix would waste memory and slow down the inner loop of Dijkstra's for no benefit.

---

### 2. Priority Queue Type for Dijkstra

**Prompt:** "What priority queue should I use for Dijkstra's algorithm in C++?"

**Original AI Response:**
"Use `std::priority_queue` with a `greater<>` comparator to get a min-heap. The default priority_queue is a max-heap, so you need to flip it. Store pairs of `(distance, vertex)` so the queue sorts by distance automatically. Declare it as `priority_queue<pair<double,TVertexID>, vector<pair<double,TVertexID>>, greater<>>`. This gives O(log V) push and pop, which is optimal for Dijkstra's."

**Changes Made:** Used `priority_queue<pair<double,TVertexID>, vector<pair<double,TVertexID>>, greater<>>` as the min-heap. Pushed `{0.0, src}` to initialize and popped minimum-distance vertices each iteration.

**Rationale:** The `greater<>` comparator flips the heap ordering so the smallest distance is always at the top, which is exactly what Dijkstra requires.

---

### 3. Stale Entry Handling in Dijkstra's Priority Queue

**Prompt:** "Dijkstra's with a priority queue can insert the same vertex multiple times. How do I handle stale entries?"

**Original AI Response:**
"Add a check at the top of the main loop: after popping `(d, u)`, compare `d` against `Dist[u]`. If `d > Dist[u]`, this is a stale entry — a shorter path to `u` was already found and processed — so just `continue` to skip it. This is the standard lazy deletion approach and avoids the need for a decrease-key operation, which `std::priority_queue` doesn't support."

**Changes Made:** Added `if(d > Dist[u]) continue;` immediately after popping from the priority queue, before the neighbor relaxation loop.

**Rationale:** Without this check, stale entries cause redundant relaxation passes and can degrade performance from O((V+E) log V) toward O(E log V) in the worst case.

---

### 4. Reconstructing the Path from Dijkstra's

**Prompt:** "How do I reconstruct the actual path after Dijkstra's finds shortest distances?"

**Original AI Response:**
"Keep a `Prev` array of size V initialized to `InvalidVertexID`. When you relax an edge `(u, v)`, set `Prev[v] = u`. After Dijkstra's finishes, trace back from `dest` to `src` by following Prev pointers. Push each vertex onto the path, then reverse it at the end. Stop when you hit `InvalidVertexID`."

**Changes Made:** Added `vector<TVertexID> Prev(N, InvalidVertexID)`. After finding the shortest distances, traced from `dest` back to `src` using Prev, pushed to path, then called `std::reverse`.

**Rationale:** The Prev array adds only O(V) memory and makes path reconstruction O(V), which is negligible compared to the O((V+E) log V) Dijkstra's pass.

---

### 5. std::any for Vertex Tags

**Prompt:** "The vertex tag is typed as `std::any`. How do I store and retrieve values of different types?"

**Original AI Response:**
"Store the tag directly by passing it to `std::any`'s constructor or assignment — it type-erases the value. To retrieve it, use `std::any_cast<T>(tag)` where T is the original type. If the cast type doesn't match, `any_cast` throws `std::bad_any_cast`. You can check the type first with `tag.type() == typeid(T)` to avoid exceptions. For your use case, callers know what type they stored, so a direct cast is fine."

**Changes Made:** Stored tags as `vector<any>` in `SImplementation`. `GetVertexTag` returns the `any` directly; callers use `any_cast` to retrieve the concrete value.

**Rationale:** `std::any` lets the router stay fully generic — the transportation planner tags vertices with OSM node IDs while the router itself doesn't need to know anything about them.

---

### 6. Pimpl Pattern for Implementation Hiding

**Prompt:** "The header declares `struct SImplementation` but doesn't define it. How do I set this up in the .cpp?"

**Original AI Response:**
"Define `struct CDijkstraPathRouter::SImplementation { ... }` fully inside the .cpp file. The header only forward-declares its existence. Initialize it in the constructor with `make_unique<SImplementation>()`. Access all state through `DImplementation->` in every method. Destructor can be `= default` in the .cpp since `unique_ptr` handles cleanup. This keeps all implementation details private to the .cpp."

**Changes Made:** Defined `SImplementation` with `Tags` and `Adj` members at the top of `DijkstraPathRouter.cpp`. All methods access state through `DImplementation->`.

**Rationale:** Pimpl keeps the header stable — changing the internal data structures doesn't require recompiling any file that includes the header.

---

### 7. Sorting Stops by ID in CBusSystemIndexer

**Prompt:** "I need stops sorted by their stop ID. What's the cleanest way to do this after loading them from CBusSystem?"

**Original AI Response:**
"Copy all stops into a `vector<shared_ptr<SStop>>` by iterating `StopByIndex` from 0 to `StopCount()`. Then call `std::sort` with a lambda comparator: `[](const auto &a, const auto &b){ return a->ID() < b->ID(); }`. This is O(n log n) and runs once at construction. For the node ID lookup map, iterate the sorted vector after sorting and insert each stop into an `unordered_map<TNodeID, shared_ptr<SStop>>`."

**Changes Made:** Populated `SortedStops` by iterating `StopByIndex`, sorted with the lambda, then built `StopByNode` by iterating the sorted result. Routes are sorted the same way by `Name()`.

**Rationale:** Sorting once at construction makes all subsequent `SortedStopByIndex` calls O(1) vector access, which is efficient for repeated queries during path planning.

---

### 8. RoutesByNodeIDs Direction Sensitivity

**Prompt:** "RoutesByNodeIDs should only match routes where src appears before dest in the stop sequence. How do I implement that?"

**Original AI Response:**
"Iterate each route's stops linearly. Use a boolean flag `foundSrc` initialized to false. When you encounter `srcStop->ID()`, set `foundSrc = true`. When `foundSrc` is true and you encounter `destStop->ID()`, add the route to the result set and break. This naturally enforces direction — if dest appears before src, foundSrc is never set when dest is reached."

**Changes Made:** Implemented the flag-based scan inside `RoutesByNodeIDs`. The `foundSrc` flag ensures only forward-direction matches count, matching the bus system's actual travel direction.

**Rationale:** Bus routes are directional — a route from stop A to stop B doesn't imply travel from B to A. The flag approach handles this correctly in a single O(stops-per-route) pass.

---

### 9. RouteBetweenNodeIDs Reusing RoutesByNodeIDs

**Prompt:** "RouteBetweenNodeIDs just checks if any route exists between two nodes. Should I duplicate the logic from RoutesByNodeIDs?"

**Original AI Response:**
"No — just call `RoutesByNodeIDs` with a local empty set and return whether the set is non-empty. This avoids duplicating logic. The only downside is a minor overhead from constructing the set, but since this is an existence check rather than a hot path, it's not worth maintaining two separate implementations."

**Changes Made:** Implemented `RouteBetweenNodeIDs` as a one-liner calling `RoutesByNodeIDs` with a local `unordered_set` and returning its result.

**Rationale:** Reusing `RoutesByNodeIDs` keeps the logic in one place — any future fix or change to the matching logic automatically applies to both methods.

---

### 10. Handling Invalid Vertex IDs in FindShortestPath

**Prompt:** "What should FindShortestPath return if src or dest are out of range?"

**Original AI Response:**
"Check both against `VertexCount()` at the top of the function and return `NoPathExists` immediately if either is invalid. Also clear the path vector before returning so callers don't see stale data from a previous call. This matches the interface contract — `NoPathExists` is the sentinel for any failure case, not just unreachable nodes."

**Changes Made:** Added `if(src >= N || dest >= N) return NoPathExists;` after `path.clear()` at the top of `FindShortestPath`.

**Rationale:** Returning early on invalid IDs prevents undefined behavior from out-of-bounds vector access and gives callers a clean, consistent failure signal.

---

### 11. Initializing Distance Vector for Dijkstra

**Prompt:** "How should I initialize the distance array before running Dijkstra's?"

**Original AI Response:**
"Initialize all distances to `NoPathExists` (which is `numeric_limits<double>::max()`). Then set `Dist[src] = 0.0`. This works naturally with the relaxation condition `if(Dist[u] + w < Dist[v])` — any real path from src will have a finite distance smaller than max, so unvisited nodes are never incorrectly relaxed. Avoid using infinity from `<cmath>` since floating point infinity has different comparison behavior than `numeric_limits::max()`."

**Changes Made:** Initialized `vector<double> Dist(N, NoPathExists)` then set `Dist[src] = 0.0`. Used `NoPathExists` from the base class rather than a magic number.

**Rationale:** Using the interface's own `NoPathExists` constant keeps the code consistent with what callers expect to receive back for unreachable nodes.

---

### 12. PortingOpenStreetMap from Proj3 — SLocation vs TLocation

**Prompt:** "I copied OpenStreetMap.cpp from Project 3 but it won't compile. The error says TLocation is unknown."

**Original AI Response:**
"Project 4's StreetMap.h changed the location type from `using TLocation = std::pair<double,double>` to a proper struct `SLocation` with named fields `DLatitude` and `DLongitude`. You need to replace all uses of `TLocation` with `SLocation` in your .cpp, and replace `std::make_pair(lat, lon)` assignments with `CStreetMap::SLocation(lat, lon)`. The struct has a two-argument constructor for exactly this purpose."

**Changes Made:** Replaced `TLocation` with `SLocation` in the node struct definition and method signature. Changed `Node->DLocation = std::make_pair(Lat, Lon)` to `CStreetMap::SLocation(Lat, Lon)`.

**Rationale:** The interface evolved between projects; the named fields `DLatitude` and `DLongitude` make the code more readable than `.first` and `.second` anyway.

---

### 13. DSVWriter Trailing Newline Behavior Change

**Prompt:** "DSVWriter tests are failing in Project 4 — the expected strings don't have a trailing newline but my implementation adds one after every row."

**Original AI Response:**
"The Project 4 tests treat newline as a separator between rows rather than a terminator after each row. Change the implementation to write the newline at the start of each row except the first. Add a boolean `DFirstRow` flag to SImplementation, initialized to true. In WriteRow, if `DFirstRow` is false, write `\n` before the row data, then set `DFirstRow = false` after the check. This produces newline-separated output with no trailing newline."

**Changes Made:** Added `bool DFirstRow = true` to `SImplementation`. In `WriteRow`, conditionally write `\n` before row data and updated the flag. Removed the trailing `\n` that was previously written after each row.

**Rationale:** The separator model matches the Project 4 test expectations and is also more natural for CSV consumers that treat the last line as complete without a terminator.

---

### 14. XMLReader CompleteElement Removal

**Prompt:** "My XMLReader returns CompleteElement for self-closing tags but the Project 4 tests expect StartElement instead. How should I fix this?"

**Original AI Response:**
"Remove the optimization that converts a StartElement to CompleteElement when immediately followed by an EndElement. Instead, always emit a proper EndElement for every closing tag. The Project 4 tests never check for CompleteElement — they expect StartElement + EndElement for all elements including self-closing ones. OpenStreetMap.cpp already handles this correctly since it checks `StartElement || CompleteElement` for element detection and uses StartElement to decide whether to read child elements."

**Changes Made:** Removed the EndElementHandler logic that converted `StartElement` to `CompleteElement`. Now always pushes a plain `EndElement`. OpenStreetMap.cpp works correctly since its `if(DType == StartElement)` branch reads child elements and immediately hits the matching EndElement.

**Rationale:** Removing CompleteElement simplifies the reader and aligns with what the tests actually expect. The OpenStreetMap parser was already written to handle both cases.

---

### 15. Deduplicating Ways with Invalid IDs in OpenStreetMap

**Prompt:** "OSMTest.ErrorTet expects WayCount() == 1 when the same way ID appears twice, but I'm getting 2."

**Original AI Response:**
"Check the `DWayByID` map before inserting a new way. If the ID already exists in the map, skip the insert entirely — don't add it to either the vector or the map. Also skip ways whose ID is `InvalidWayID` (set when parsing failed). Combine both checks: `if(Way->DID != InvalidWayID && DWayByID.find(Way->DID) == DWayByID.end())`. Only insert into both the vector and the map inside that conditional."

**Changes Made:** Wrapped the way insertion block in the combined conditional. Removed the separate `if(Way->DID != InvalidWayID)` map-only check that previously allowed duplicate ways into the vector.

**Rationale:** The map lookup is O(1) and the deduplication keeps the data structure consistent — the vector index stored in the map only makes sense if each way appears exactly once in the vector.

---

### 16. Tracking Visited Vertices

**Prompt:** "Do I need a visited set when implementing Dijkstra’s algorithm?"

**Original AI Response:**
"Yes, it is common to keep a structure that tracks which vertices have already been finalized. When a vertex is removed from the priority queue with the smallest tentative distance, that distance is guaranteed to be the shortest possible for that vertex. At that point the vertex can be marked as visited so it is not processed again. This avoids unnecessary work and ensures that each vertex is finalized only once."

**Changes Made:** 
Used a structure to track which vertices had already been processed during the algorithm.

**Rationale:** 
Tracking visited vertices prevents unnecessary reprocessing and ensures the algorithm follows the standard behavior of finalizing the shortest distance for each vertex once.

---

### 17. Storing Distances

**Prompt:** "What is a good way to store the distance values for each vertex?"

**Original AI Response:**
"You should maintain a container that maps each vertex to its current best-known distance from the source. In C++, this is often done using a map or unordered_map keyed by the vertex ID. Initially, all vertices are assigned a very large value representing infinity, except the source which is set to zero. Whenever the algorithm discovers a shorter path to a vertex, the stored distance should be updated."

**Changes Made:** 
Maintained a data structure that tracked the current shortest distance for each vertex during the algorithm.

**Rationale:**
 Dijkstra’s algorithm relies on comparing tentative distances and updating them when shorter paths are found, so storing these values is necessary for the algorithm to function correctly.

---

### 18. Mising Dependencies

**Prompt:** I keep getting the return error of "[Makefile:90: testbin/testtpcl] Error 1" (copied entire terminal message)

**Original AI Response:**
I identified that obj/GeographicUtils.o was missing from the testbin/testtpcl link rule, causing an undefined reference to SGeographicUtils::ConvertLLToDMS at link time. Add obj/GeographicUtils.o to the list of object files in that rule.

**Changes Made:** 
Added obj/GeographicUtils.o to the testbin/testtpcl rule in the Makefile so that the geographic utilities object file is included when linking the command-line test binary.

**Rationale:** 
The TransportationPlannerCommandLine implementation calls SGeographicUtils::ConvertLLToDMS to format node coordinates. Without linking GeographicUtils.o, the linker cannot resolve that symbol, causing a build failure even though the source code itself is correct.

---