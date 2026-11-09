#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace gatorevac {

class KdTree2D {
public:
    struct Result {
        uint32_t nodeId = 0;
        double dist2 = std::numeric_limits<double>::infinity();
    };

    void build(const std::vector<double>& xs, const std::vector<double>& ys,
               const std::vector<uint32_t>& nodeIds = {});

    Result nearest(double x, double y) const;

private:
    struct Node {
        double x = 0, y = 0;
        uint32_t nodeId = 0;
        int32_t left = -1, right = -1;
        uint8_t axis = 0;
    };

    int32_t buildRange(std::vector<uint32_t>& order, uint32_t lo, uint32_t hi, int depth,
                       const std::vector<double>& xs, const std::vector<double>& ys,
                       const std::vector<uint32_t>& nodeIds);
    void search(int32_t node, double x, double y, Result& best) const;

    std::vector<Node> nodes_;
    int32_t root_ = -1;
};

}
