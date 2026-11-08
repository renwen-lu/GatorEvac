#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "geo.h"

namespace gatorevac {

class Graph {
public:
    struct Edge {
        uint32_t to;
        uint32_t weight;
        bool closed = false;
    };

    static Graph loadDimacs(const std::string& grPath, const std::string& coPath);
    uint32_t numNodes() const { return static_cast<uint32_t>(lon_.size()); }
    uint32_t numArcs() const { return arcCount_; }
    const std::vector<Edge>& neighbors(uint32_t v) const { return adj_[v]; }
    std::vector<Edge>& neighbors(uint32_t v) { return adj_[v]; }
    void openAll();
    uint32_t numClosed() const;

    double lon(uint32_t v) const { return lon_[v]; }
    double lat(uint32_t v) const { return lat_[v]; }
    double x(uint32_t v) const { return x_[v]; }
    double y(uint32_t v) const { return y_[v]; }
    const std::vector<double>& xs() const { return x_; }
    const std::vector<double>& ys() const { return y_; }
    const Projection& projection() const { return proj_; }

    // Scale for the A* estimate.
    double minWeightPerMeter() const { return minWeightPerMeter_; }

private:
    void computeProjectionAndScale();

    std::vector<std::vector<Edge>> adj_;
    uint32_t arcCount_ = 0;
    std::vector<double> lon_, lat_, x_, y_;
    Projection proj_;
    double minWeightPerMeter_ = 0.0;
};

}
