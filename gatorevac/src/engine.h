#pragma once

#include <string>
#include <vector>

#include "graph.h"
#include "kd_tree.h"
#include "router.h"
#include "scenario.h"

namespace gatorevac {

class Engine {
public:
    explicit Engine(const std::string& dataDir);

    Graph& graph() { return graph_; }
    Scenario& scenario() { return scenario_; }
    Router& router() { return router_; }

    double pathKm(const std::vector<uint32_t>& path) const;

private:
    Graph graph_;
    KdTree2D nodeTree_;
    Scenario scenario_;
    Router router_;
};

}
