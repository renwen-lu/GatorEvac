#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "graph.h"
#include "kd_tree.h"

namespace gatorevac {

struct Shelter {
    std::string name;
    std::string county;
    std::string city;
    double lat = 0, lon = 0;
    int capacity = 0;
    uint32_t node = 0;
    bool userOpen = true;
    bool flooded = false;
    bool offNetwork = false;
    bool isOpen() const { return userOpen && !flooded && !offNetwork; }
};

struct FloodZone {
    double lat = 0, lon = 0;
    double radiusKm = 0;
};

class Scenario {
public:
    static constexpr double kMaxSnapMeters = 5000.0;

    Scenario(Graph& graph, const KdTree2D& nodeTree);

    void loadShelters(const std::string& csvPath);

    const std::vector<Shelter>& shelters() const { return shelters_; }
    void setShelterOpen(std::size_t index, bool open);

    const std::vector<FloodZone>& floodZones() const { return zones_; }
    void reset();
    void addFloodZone(const FloodZone& z);

    const std::vector<uint8_t>& isGoal() const { return isGoal_; }
    const KdTree2D& goalTree() const { return goalTree_; }
    std::size_t numOpen() const;

    int shelterAtNode(uint32_t node) const;
    uint32_t snap(double lon, double lat) const;

private:
    static bool startsOpen(const Shelter& s);
    void rebuild();

    Graph& g_;
    const KdTree2D& nodeTree_;
    std::vector<Shelter> shelters_;
    std::vector<FloodZone> zones_;
    std::vector<uint8_t> isGoal_;
    KdTree2D goalTree_;
};

}
