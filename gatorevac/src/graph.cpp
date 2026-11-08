#include "graph.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace gatorevac {

Graph Graph::loadDimacs(const string& grPath, const string& coPath) {
    ifstream gr(grPath);
    if (!gr) throw runtime_error("cannot open " + grPath);
    Graph g;
    uint32_t n = 0;
    string line;
    while (getline(gr, line)) {
        if (line.empty() || line[0] == 'c') continue;
        istringstream row(line);
        char type;
        row >> type;
        if (line[0] == 'p') {
            string format;
            row >> format >> n;
            g.adj_.resize(n);
        } else if (line[0] == 'a') {
            uint32_t from, to, weight;
            row >> from >> to >> weight;
            g.adj_[from - 1].push_back({to - 1, weight});
            ++g.arcCount_;
        }
    }

    ifstream co(coPath);
    if (!co) throw runtime_error("cannot open " + coPath);
    g.lon_.resize(n);
    g.lat_.resize(n);
    while (getline(co, line)) {
        if (line.empty() || line[0] != 'v') continue;
        istringstream row(line);
        char type;
        uint32_t id;
        double x, y;
        row >> type >> id >> x >> y;
        g.lon_[id - 1] = x / 1e6;
        g.lat_[id - 1] = y / 1e6;
    }

    g.computeProjectionAndScale();
    return g;
}

void Graph::computeProjectionAndScale() {
    const uint32_t n = numNodes();
    double lo = lat_[0], hi = lat_[0];
    for (double v : lat_) {
        lo = min(lo, v);
        hi = max(hi, v);
    }
    proj_.refLatDeg = 0.5 * (lo + hi);
    x_.resize(n);
    y_.resize(n);
    for (uint32_t v = 0; v < n; ++v) {
        x_[v] = proj_.x(lon_[v]);
        y_[v] = proj_.y(lat_[v]);
    }
    double minRatio = numeric_limits<double>::infinity();
    for (uint32_t u = 0; u < n; ++u) {
        for (const Edge& edge : adj_[u]) {
            const double len = planarDistance(x_[u], y_[u], x_[edge.to], y_[edge.to]);
            if (len <= 0.0) continue;
            const double r = static_cast<double>(edge.weight) / len;
            minRatio = min(minRatio, r);
        }
    }
    // Use the smallest ratio so A* does not overestimate.
    minWeightPerMeter_ = minRatio;
}

void Graph::openAll() {
    for (auto& list : adj_)
        for (Edge& edge : list)
            edge.closed = false;
}

uint32_t Graph::numClosed() const {
    uint32_t c = 0;
    for (const auto& list : adj_)
        for (const Edge& edge : list)
            c += edge.closed;
    return c;
}

}
