#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "graph.h"
#include "indexed_min_heap.h"
#include "kd_tree.h"

namespace gatorevac {

enum class Algorithm { Dijkstra, AStar };
const char* algorithmName(Algorithm a);

struct SearchStats {
    uint64_t settled = 0;
    uint64_t relaxed = 0;
    double millis = 0.0;
};

struct SearchResult {
    bool found = false;
    uint32_t source = 0;
    uint32_t target = 0;
    uint64_t distance = 0;
    std::vector<uint32_t> path;
    SearchStats stats;
};

class Router {
public:
    static constexpr uint64_t kInf = std::numeric_limits<uint64_t>::max();
    explicit Router(const Graph& graph);

    SearchResult route(Algorithm algo, uint32_t source, uint32_t target,
                       std::vector<uint32_t>* visitOrder = nullptr);

    SearchResult routeToNearest(Algorithm algo, uint32_t source, const std::vector<uint8_t>& isGoal,
                                const KdTree2D& goalTree, std::vector<uint32_t>* visitOrder = nullptr);

private:
    SearchResult search(Algorithm algo, uint32_t source, uint32_t target, const std::vector<uint8_t>* goals,
                        const KdTree2D* goalTree, std::vector<uint32_t>* visitOrder);
    double getHeuristic(uint32_t node, uint32_t target, const KdTree2D* goalTree) const;

    const Graph& g_;
    double scale_;
    IndexedMinHeap heap_;
    std::vector<uint64_t> dist_;
    std::vector<uint32_t> parent_;
};

}
