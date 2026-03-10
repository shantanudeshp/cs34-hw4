# CBusSystemIndexer

Wraps a `CBusSystem` and provides sorted, indexed access to stops and routes, plus fast lookup by node ID and route connectivity queries.

## Construction

```cpp
auto busSystem = std::make_shared<CCSVBusSystem>(stopReader, routeReader);
CBusSystemIndexer indexer(busSystem);
```

On construction, stops are sorted by stop ID and routes are sorted alphabetically by name. A node ID → stop lookup map is built for O(1) access.

## Sorted Access

```cpp
std::size_t n = indexer.StopCount();   // total stops
std::size_t r = indexer.RouteCount();  // total routes

auto stop  = indexer.SortedStopByIndex(0);   // stop with lowest ID
auto route = indexer.SortedRouteByIndex(0);  // alphabetically first route
```

Returns `nullptr` if index is out of range.

## Lookup by Node ID

```cpp
auto stop = indexer.StopByNodeID(nodeID);
if(stop){
    std::cout << "Stop ID: " << stop->ID() << "\n";
}
```

Returns `nullptr` if no stop is at that OSM node.

## Route Queries

```cpp
std::unordered_set<std::shared_ptr<CBusSystem::SRoute>> routes;
bool found = indexer.RoutesByNodeIDs(srcNode, destNode, routes);
// routes contains all routes with a segment from srcNode's stop to destNode's stop

bool connected = indexer.RouteBetweenNodeIDs(srcNode, destNode);
```

`RoutesByNodeIDs` considers order — a route qualifies only if `src`'s stop appears before `dest`'s stop in the route's stop sequence.

## Example

```cpp
auto stop0 = indexer.SortedStopByIndex(0);
auto stop1 = indexer.SortedStopByIndex(1);

std::unordered_set<std::shared_ptr<CBusSystem::SRoute>> routes;
if(indexer.RoutesByNodeIDs(stop0->NodeID(), stop1->NodeID(), routes)){
    for(auto &r : routes)
        std::cout << r->Name() << "\n";
}
```
