#include "router.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace std;

namespace gatorevac {

const char* algorithmName(Algorithm a) {
    if (a == Algorithm::Dijkstra) return "dijkstra";
    return "astar";
}

Router::Router(const Graph& graph)
    : g_(graph),
      scale_(graph.minWeightPerMeter()), heap_(graph.numNodes()),
      dist_(graph.numNodes()), parent_(graph.numNodes()) {
}

double Router::getHeuristic(uint32_t node, uint32_t target, const KdTree2D* goalTree) const {
    if (goalTree != nullptr) {
        const KdTree2D::Result nearest = goalTree->nearest(g_.x(node), g_.y(node));
        return scale_ * sqrt(nearest.dist2);
    }
    return scale_ * planarDistance(g_.x(node), g_.y(node), g_.x(target), g_.y(target));
}

SearchResult Router::search(Algorithm algo, uint32_t source, uint32_t target, const vector<uint8_t>* goals,
                            const KdTree2D* goalTree, vector<uint32_t>* visitOrder) {
    const auto start = chrono::steady_clock::now();
    heap_.clear();
    fill(dist_.begin(), dist_.end(), kInf);
    if (visitOrder) visitOrder->clear();
    SearchResult res;
    res.source = source;

    dist_[source] = 0;
    parent_[source] = source;
    double priority = 0.0;
    if (algo == Algorithm::AStar) {
        priority = getHeuristic(source, target, goalTree);
    }
    heap_.push(source, priority);

    while (!heap_.empty()) {
        const uint32_t u = heap_.pop();
        ++res.stats.settled;
        if (visitOrder) visitOrder->push_back(u);
        const bool reached = goals ? (*goals)[u] != 0 : u == target;
        if (reached) {
            res.found = true;
            res.target = u;
            res.distance = dist_[u];
            break;
        }
        const uint64_t du = dist_[u];
        for (const Graph::Edge& edge : g_.neighbors(u)) {
            if (edge.closed) continue;
            const uint32_t v = edge.to;
            const uint64_t nd = du + edge.weight;
            if (nd < dist_[v]) {
                dist_[v] = nd;
                parent_[v] = u;
                // Dijkstra uses the cost so far; A* also adds an estimate.
                priority = static_cast<double>(nd);
                if (algo == Algorithm::AStar) {
                    priority += getHeuristic(v, target, goalTree);
                }
                heap_.pushOrDecrease(v, priority);
                ++res.stats.relaxed;
            }
        }
    }

    if (res.found) {
        for (uint32_t v = res.target;; v = parent_[v]) {
            res.path.push_back(v);
            if (v == source) break;
        }
        reverse(res.path.begin(), res.path.end());
    }
    res.stats.millis = chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
    return res;
}

SearchResult Router::route(Algorithm algo, uint32_t source, uint32_t target, vector<uint32_t>* visitOrder) {
    return search(algo, source, target, nullptr, nullptr, visitOrder);
}

SearchResult Router::routeToNearest(Algorithm algo, uint32_t source, const vector<uint8_t>& isGoal,
                                    const KdTree2D& goalTree, vector<uint32_t>* visitOrder) {
    return search(algo, source, 0, &isGoal, &goalTree, visitOrder);
}

}
