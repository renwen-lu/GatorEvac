#include "scenario.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace std;

namespace gatorevac {

namespace {

vector<string> splitCsv(const string& line) {
    vector<string> out;
    string cell;
    istringstream ss(line);
    while (getline(ss, cell, ','))
        out.push_back(cell);
    return out;
}

string upper(string s) {
    for (char& ch : s)
        ch = toupper(ch);
    return s;
}

}

Scenario::Scenario(Graph& graph, const KdTree2D& nodeTree)
    : g_(graph), nodeTree_(nodeTree), isGoal_(graph.numNodes(), 0) {
}

uint32_t Scenario::snap(double lon, double lat) const {
    const Projection& p = g_.projection();
    return nodeTree_.nearest(p.x(lon), p.y(lat)).nodeId;
}

void Scenario::loadShelters(const string& csvPath) {
    ifstream in(csvPath);
    if (!in) throw runtime_error("cannot open " + csvPath);
    string line;
    getline(in, line);

    shelters_.clear();
    const Projection& p = g_.projection();
    while (getline(in, line)) {
        if (line.empty()) continue;
        const vector<string> f = splitCsv(line);
        Shelter s;
        s.name = f[1];
        s.county = f[2];
        s.city = f[3];
        s.lat = stod(f[4]);
        s.lon = stod(f[5]);
        s.capacity = stoi(f[6]);
        const KdTree2D::Result r = nodeTree_.nearest(p.x(s.lon), p.y(s.lat));
        s.node = r.nodeId;
        const double snapMeters = sqrt(r.dist2);
        s.offNetwork = snapMeters > kMaxSnapMeters;
        s.userOpen = startsOpen(s);
        shelters_.push_back(move(s));
    }
    rebuild();
}

void Scenario::setShelterOpen(size_t index, bool open) {
    shelters_[index].userOpen = open;
    rebuild();
}

bool Scenario::startsOpen(const Shelter& s) {
    return upper(s.name).find("INACTIVE") == string::npos;
}

void Scenario::reset() {
    for (Shelter& s : shelters_)
        s.userOpen = startsOpen(s);
    zones_.clear();
    rebuild();
}

void Scenario::addFloodZone(const FloodZone& z) {
    if (z.radiusKm <= 0) throw invalid_argument("flood radius must be positive");
    zones_.push_back(z);
    rebuild();
}

void Scenario::rebuild() {
    const Projection& p = g_.projection();
    const uint32_t n = g_.numNodes();
    struct Circle {
        double x, y, r;
    };
    vector<Circle> circles;
    for (const FloodZone& z : zones_)
        circles.push_back({p.x(z.lon), p.y(z.lat), z.radiusKm * 1000.0});
    auto inside = [&circles](double x, double y) {
        for (const Circle& c : circles)
            if (planarDistance(x, y, c.x, c.y) <= c.r) return true;
        return false;
    };

    // Check the whole road segment.
    g_.openAll();
    if (!circles.empty()) {
        for (uint32_t u = 0; u < n; ++u) {
            for (Graph::Edge& edge : g_.neighbors(u)) {
                const uint32_t v = edge.to;
                for (const Circle& c : circles) {
                    if (pointSegmentDistance(c.x, c.y, g_.x(u), g_.y(u), g_.x(v), g_.y(v)) <= c.r) {
                        edge.closed = true;
                        break;
                    }
                }
            }
        }
    }
    for (Shelter& s : shelters_)
        s.flooded = inside(p.x(s.lon), p.y(s.lat)) || inside(g_.x(s.node), g_.y(s.node));

    fill(isGoal_.begin(), isGoal_.end(), 0);
    vector<double> xs, ys;
    vector<uint32_t> nodes;
    for (const Shelter& s : shelters_) {
        if (!s.isOpen() || isGoal_[s.node]) continue;
        isGoal_[s.node] = 1;
        // Use road-node coordinates for A*.
        xs.push_back(g_.x(s.node));
        ys.push_back(g_.y(s.node));
        nodes.push_back(s.node);
    }
    goalTree_.build(xs, ys, nodes);
}

size_t Scenario::numOpen() const {
    size_t c = 0;
    for (const Shelter& s : shelters_)
        c += s.isOpen() ? 1 : 0;
    return c;
}

int Scenario::shelterAtNode(uint32_t node) const {
    for (size_t i = 0; i < shelters_.size(); ++i)
        if (shelters_[i].node == node && shelters_[i].isOpen()) return static_cast<int>(i);
    return -1;
}

}
