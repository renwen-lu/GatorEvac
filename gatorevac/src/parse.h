#pragma once

#include <stdexcept>
#include <string>

namespace gatorevac {

inline void parseLatLon(const std::string& s, double& lat, double& lon) {
    const auto comma = s.find(',');
    if (comma == std::string::npos) throw std::invalid_argument("expected LAT,LON");
    lat = std::stod(s.substr(0, comma));
    lon = std::stod(s.substr(comma + 1));
}

}
