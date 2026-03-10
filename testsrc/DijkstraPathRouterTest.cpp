#include <gtest/gtest.h>
#include "DijkstraPathRouter.h"

TEST(DijkstraPathRouter, SimpleTest){
    CDijkstraPathRouter Router;
    EXPECT_EQ(Router.VertexCount(), 0);
    auto V0 = Router.AddVertex(std::string("A"));
    auto V1 = Router.AddVertex(std::string("B"));
    auto V2 = Router.AddVertex(std::string("C"));
    EXPECT_EQ(Router.VertexCount(), 3);
    EXPECT_EQ(std::any_cast<std::string>(Router.GetVertexTag(V0)), "A");
    EXPECT_EQ(std::any_cast<std::string>(Router.GetVertexTag(V1)), "B");
    EXPECT_TRUE(Router.AddEdge(V0, V1, 1.0));
    EXPECT_TRUE(Router.AddEdge(V1, V2, 2.0));
    EXPECT_FALSE(Router.AddEdge(V0, V1, -1.0));
    std::vector<CPathRouter::TVertexID> Path;
    EXPECT_DOUBLE_EQ(Router.FindShortestPath(V0, V2, Path), 3.0);
    EXPECT_EQ(Path, (std::vector<CPathRouter::TVertexID>{V0, V1, V2}));
}

TEST(DijkstraPathRouter, NoPathTest){
    CDijkstraPathRouter Router;
    auto V0 = Router.AddVertex(0);
    auto V1 = Router.AddVertex(1);
    std::vector<CPathRouter::TVertexID> Path;
    EXPECT_EQ(Router.FindShortestPath(V0, V1, Path), CPathRouter::NoPathExists);
    EXPECT_TRUE(Path.empty());
}

TEST(DijkstraPathRouter, BidirTest){
    CDijkstraPathRouter Router;
    auto V0 = Router.AddVertex(0);
    auto V1 = Router.AddVertex(1);
    Router.AddEdge(V0, V1, 5.0, true);
    std::vector<CPathRouter::TVertexID> Path;
    EXPECT_DOUBLE_EQ(Router.FindShortestPath(V1, V0, Path), 5.0);
    EXPECT_EQ(Path, (std::vector<CPathRouter::TVertexID>{V1, V0}));
}

TEST(DijkstraPathRouter, ShortestNotFirstTest){
    CDijkstraPathRouter Router;
    auto V0 = Router.AddVertex(0);
    auto V1 = Router.AddVertex(1);
    auto V2 = Router.AddVertex(2);
    Router.AddEdge(V0, V2, 10.0);
    Router.AddEdge(V0, V1, 1.0);
    Router.AddEdge(V1, V2, 2.0);
    std::vector<CPathRouter::TVertexID> Path;
    EXPECT_DOUBLE_EQ(Router.FindShortestPath(V0, V2, Path), 3.0);
    EXPECT_EQ(Path, (std::vector<CPathRouter::TVertexID>{V0, V1, V2}));
}
