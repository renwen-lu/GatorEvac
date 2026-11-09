#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "engine.h"
#include "parse.h"
#include "httplib.h"

using namespace std;
using namespace gatorevac;
using httplib::Request;
using httplib::Response;

namespace {

string jstr(const string& s) {
    string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

void json(Response& r, const string& text) {
    r.set_content(text, "application/json");
}

void error(Response& r, int status, const string& message) {
    r.status = status;
    json(r, "{\"error\":" + jstr(message) + "}");
}

void binary(Response& r, const void* data, size_t bytes) {
    r.set_content(static_cast<const char*>(data), bytes, "application/octet-stream");
}

string num(double v, int decimals) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

string jlist(const vector<string>& items) {
    string out = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += ',';
        out += items[i];
    }
    return out + "]";
}

class WebApp {
public:
    explicit WebApp(Engine& e) : e_(e) {
        const Graph& g = e_.graph();
        for (uint32_t v = 0; v < g.numNodes(); ++v) {
            nodes_.push_back(static_cast<float>(g.lon(v)));
            nodes_.push_back(static_cast<float>(g.lat(v)));
        }
    }

    void install(httplib::Server& s) {
        s.Get("/api/info", [this](const Request&, Response& r) { info(r); });
        s.Get("/api/nodes.bin", [this](const Request&, Response& r) {
            binary(r, nodes_.data(), nodes_.size() * sizeof(float));
        });
        s.Get("/api/roads.bin", [this](const Request&, Response& r) { roads(r, false); });
        s.Get("/api/closed.bin", [this](const Request&, Response& r) { roads(r, true); });
        s.Get("/api/shelters", [this](const Request&, Response& r) { shelters(r); });
        s.Get("/api/route", [this](const Request& q, Response& r) { route(q, r); });
        s.Get("/api/explored", [this](const Request& q, Response& r) { explored(q, r); });
        s.Post("/api/shelter", [this](const Request& q, Response& r) { setShelter(q, r); });
        s.Post("/api/flood", [this](const Request& q, Response& r) { addFlood(q, r); });
        s.Post("/api/reset", [this](const Request&, Response& r) {
            e_.scenario().reset();
            json(r, scenarioJson());
        });
    }

private:
    string scenarioJson() const {
        const Scenario& sc = e_.scenario();
        vector<string> state;
        for (const Shelter& s : sc.shelters()) {
            if (s.offNetwork)
                state.push_back(jstr("off_network"));
            else if (s.flooded)
                state.push_back(jstr("flooded"));
            else if (s.userOpen)
                state.push_back(jstr("open"));
            else
                state.push_back(jstr("closed"));
        }
        vector<string> zones;
        for (const FloodZone& z : sc.floodZones())
            zones.push_back("{\"lat\":" + num(z.lat, 6) + ",\"lon\":" + num(z.lon, 6) +
                            ",\"km\":" + num(z.radiusKm, 2) + "}");
        return "{\"closedArcs\":" + to_string(e_.graph().numClosed()) +
               ",\"openShelters\":" + to_string(sc.numOpen()) + ",\"zones\":" + jlist(zones) +
               ",\"state\":" + jlist(state) + "}";
    }

    void setShelter(const Request& q, Response& r) {
        const size_t i = stoul(q.get_param_value("i"));
        if (i >= e_.scenario().shelters().size()) return error(r, 400, "bad shelter index");
        e_.scenario().setShelterOpen(i, q.get_param_value("open") == "1");
        json(r, scenarioJson());
    }

    void addFlood(const Request& q, Response& r) {
        const double lat = stod(q.get_param_value("lat"));
        const double lon = stod(q.get_param_value("lon"));
        const double km = stod(q.get_param_value("km"));
        e_.scenario().addFloodZone({lat, lon, km});
        json(r, scenarioJson());
    }

    void info(Response& r) const {
        const Graph& g = e_.graph();
        json(r, "{\"nodes\":" + to_string(g.numNodes()) + ",\"arcs\":" + to_string(g.numArcs()) +
               ",\"scenario\":" + scenarioJson() + "}");
    }

    bool hasArc(uint32_t from, uint32_t to) const {
        const Graph& g = e_.graph();
        for (const Graph::Edge& edge : g.neighbors(from))
            if (edge.to == to) return true;
        return false;
    }

    void roads(Response& r, bool closedOnly) const {
        const Graph& g = e_.graph();
        vector<uint32_t> out;
        for (uint32_t u = 0; u < g.numNodes(); ++u) {
            for (const Graph::Edge& edge : g.neighbors(u)) {
                const uint32_t v = edge.to;
                if (closedOnly && !edge.closed) continue;
                if (u < v || !hasArc(v, u)) {
                    out.push_back(u);
                    out.push_back(v);
                }
            }
        }
        binary(r, out.data(), out.size() * sizeof(uint32_t));
    }

    void shelters(Response& r) const {
        vector<string> items;
        for (const Shelter& s : e_.scenario().shelters())
            items.push_back("{\"name\":" + jstr(s.name) + ",\"county\":" + jstr(s.county) +
                            ",\"city\":" + jstr(s.city) + ",\"lat\":" + num(s.lat, 6) +
                            ",\"lon\":" + num(s.lon, 6) + ",\"cap\":" + to_string(s.capacity) + "}");
        json(r, jlist(items));
    }

    string nodeJson(uint32_t v) const {
        return "{\"node\":" + to_string(v) + ",\"lat\":" + num(e_.graph().lat(v), 6) +
               ",\"lon\":" + num(e_.graph().lon(v), 6) + "}";
    }

    string resultJson(const SearchResult& res, int shelter) const {
        vector<string> path;
        for (uint32_t v : res.path)
            path.push_back("[" + num(e_.graph().lon(v), 6) + "," + num(e_.graph().lat(v), 6) + "]");
        return string("{\"found\":") + (res.found ? "true" : "false") +
               ",\"distance\":" + to_string(res.distance) + ",\"km\":" + num(e_.pathKm(res.path), 3) +
               ",\"settled\":" + to_string(res.stats.settled) +
               ",\"ms\":" + num(res.stats.millis, 3) + ",\"shelter\":" + to_string(shelter) +
               ",\"target\":" + nodeJson(res.target) + ",\"path\":" + jlist(path) + "}";
    }

    void route(const Request& q, Response& r) {
        double lat = 0, lon = 0, tlat = 0, tlon = 0;
        const bool pick = q.has_param("to");
        parseLatLon(q.get_param_value("from"), lat, lon);
        if (pick) parseLatLon(q.get_param_value("to"), tlat, tlon);

        Scenario& sc = e_.scenario();
        Router& router = e_.router();
        const uint32_t source = sc.snap(lon, lat);
        const uint32_t target = pick ? sc.snap(tlon, tlat) : 0;
        vector<string> results;
        const Algorithm algorithms[] = {Algorithm::Dijkstra, Algorithm::AStar};
        for (int i = 0; i < 2; ++i) {
            const Algorithm a = algorithms[i];
            SearchResult res;
            if (pick) {
                res = router.route(a, source, target, &order_[i]);
            } else {
                res = router.routeToNearest(a, source, sc.isGoal(), sc.goalTree(), &order_[i]);
            }
            int shelter = -1;
            if (res.found && !pick) shelter = sc.shelterAtNode(res.target);
            results.push_back(resultJson(res, shelter));
        }
        json(r, "{\"origin\":" + nodeJson(source) +
               ",\"dest\":" + (pick ? nodeJson(target) : "null") + ",\"results\":" + jlist(results) + "}");
    }

    void explored(const Request& q, Response& r) const {
        const vector<uint32_t>& o = order_[q.get_param_value("algo") == "1" ? 1 : 0];
        binary(r, o.data(), o.size() * sizeof(uint32_t));
    }

    Engine& e_;
    vector<float> nodes_;
    vector<uint32_t> order_[2];
};

}

int main(int argc, char** argv) {
    string dataDir = "data";
    int port = 8080;
    try {
        for (int i = 1; i < argc; ++i) {
            const string a = argv[i];
            if (a == "--data" && i + 1 < argc) {
                dataDir = argv[++i];
            } else if (a == "--port" && i + 1 < argc) {
                port = stoi(argv[++i]);
            } else {
                cerr << "usage: gatorevac-web [--data DIR] [--port PORT]\n";
                return 1;
            }
        }
        Engine engine(dataDir);
        WebApp app(engine);
        httplib::Server server;
        // One worker shares the router and scenario.
        server.new_task_queue = [] { return new httplib::ThreadPool(1); };
        // Keep idle connections from holding the only worker.
        server.set_keep_alive_max_count(1);
        server.set_keep_alive_timeout(2);
        server.set_default_headers({{"Cache-Control", "no-store"}});
        server.set_error_handler([](const Request&, Response& r) {
            if (r.body.empty()) error(r, r.status, "request failed");
        });
        server.set_mount_point("/", "web");
        app.install(server);
        const string url = "http://127.0.0.1:" + to_string(port) + "/";
        cout << "\nOpen " << url << " in your browser.\nKeep this window open while using the map." << endl;
        if (!server.listen("127.0.0.1", port)) {
            cerr << "error: could not listen on " << url << '\n';
            return 1;
        }
    } catch (const exception& e) {
        cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
