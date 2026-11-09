#include "engine.h"

#include <iostream>

using namespace std;

namespace gatorevac {

Engine::Engine(const string& dataDir)
    : graph_(Graph::loadDimacs(dataDir + "/USA-road-d.FLA.gr", dataDir + "/USA-road-d.FLA.co")),
      scenario_(graph_, nodeTree_), router_(graph_) {
    nodeTree_.build(graph_.xs(), graph_.ys());
    scenario_.loadShelters(dataDir + "/shelters_fl.csv");
    cout << "Graph: " << graph_.numNodes() << " intersections, " << graph_.numArcs()
         << " directed arcs; " << scenario_.shelters().size() << " shelters ("
         << scenario_.numOpen() << " open)\n";
}

double Engine::pathKm(const vector<uint32_t>& path) const {
    double m = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        const uint32_t a = path[i - 1], b = path[i];
        m += haversineMeters(graph_.lat(a), graph_.lon(a), graph_.lat(b), graph_.lon(b));
    }
    return m / 1000.0;
}

}
