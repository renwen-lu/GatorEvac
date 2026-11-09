#include "kd_tree.h"

#include <algorithm>

using namespace std;

namespace gatorevac {

void KdTree2D::build(const vector<double>& xs, const vector<double>& ys, const vector<uint32_t>& nodeIds) {
    nodes_.clear();
    nodes_.reserve(xs.size());
    vector<uint32_t> order(xs.size());
    for (uint32_t i = 0; i < order.size(); ++i)
        order[i] = i;
    root_ = buildRange(order, 0, static_cast<uint32_t>(order.size()), 0, xs, ys, nodeIds);
}

int32_t KdTree2D::buildRange(vector<uint32_t>& order, uint32_t lo, uint32_t hi, int depth,
                             const vector<double>& xs, const vector<double>& ys,
                             const vector<uint32_t>& nodeIds) {
    if (lo >= hi) return -1;
    const int axis = depth % 2;
    const uint32_t mid = lo + (hi - lo) / 2;
    sort(order.begin() + lo, order.begin() + hi, [&](uint32_t a, uint32_t b) {
        if (axis == 0) return xs[a] < xs[b];
        return ys[a] < ys[b];
    });

    const uint32_t p = order[mid];
    const int32_t self = static_cast<int32_t>(nodes_.size());
    Node node;
    node.x = xs[p];
    node.y = ys[p];
    node.nodeId = nodeIds.empty() ? p : nodeIds[p];
    node.axis = static_cast<uint8_t>(axis);
    nodes_.push_back(node);

    const int32_t left = buildRange(order, lo, mid, depth + 1, xs, ys, nodeIds);
    const int32_t right = buildRange(order, mid + 1, hi, depth + 1, xs, ys, nodeIds);
    nodes_[self].left = left;
    nodes_[self].right = right;
    return self;
}

KdTree2D::Result KdTree2D::nearest(double x, double y) const {
    Result best;
    if (root_ >= 0) search(root_, x, y, best);
    return best;
}

void KdTree2D::search(int32_t idx, double x, double y, Result& best) const {
    const Node& n = nodes_[idx];
    const double dx = n.x - x, dy = n.y - y;
    const double d2 = dx * dx + dy * dy;
    if (d2 < best.dist2) {
        best.dist2 = d2;
        best.nodeId = n.nodeId;
    }
    const double diff = (n.axis == 0) ? (x - n.x) : (y - n.y);
    const int32_t nearSide = diff < 0 ? n.left : n.right;
    const int32_t farSide = diff < 0 ? n.right : n.left;
    if (nearSide >= 0) search(nearSide, x, y, best);
    // Only check the far side if it could have a closer point.
    if (farSide >= 0 && diff * diff < best.dist2) search(farSide, x, y, best);
}

}
