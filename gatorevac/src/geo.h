#pragma once

#include <cmath>

namespace gatorevac {

constexpr double kEarthRadiusM = 6371000.0;
constexpr double kPi = 3.141592653589793;

inline double degToRad(double deg) {
    return deg * kPi / 180.0;
}

struct Projection {
    double refLatDeg = 0.0;

    double x(double lonDeg) const { return degToRad(lonDeg) * std::cos(degToRad(refLatDeg)) * kEarthRadiusM; }
    double y(double latDeg) const { return degToRad(latDeg) * kEarthRadiusM; }
};

inline double planarDistance(double ax, double ay, double bx, double by) {
    return std::hypot(ax - bx, ay - by);
}

inline double pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

inline double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
    const double p1 = degToRad(lat1), p2 = degToRad(lat2);
    const double dp = p2 - p1, dl = degToRad(lon2 - lon1);
    const double a = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadiusM * std::asin(std::sqrt(a));
}

}
