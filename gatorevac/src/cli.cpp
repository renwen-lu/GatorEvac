#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "engine.h"
#include "parse.h"

using namespace std;
using namespace gatorevac;

namespace {

struct Options {
    string command;
    string dataDir = "data";
    string out;
    bool hasFrom = false, hasTo = false;
    double fromLat = 0, fromLon = 0, toLat = 0, toLon = 0;
    string algo = "both";
    int queries = 0;
    unsigned seed = 42;
    vector<FloodZone> floods;
};

void usage() {
    cout << "usage: gatorevac-cli <route|verify|bench> [options]\n"
         << "  --data DIR            data directory (default: data)\n"
         << "  --from LAT,LON        origin (route)\n"
         << "  --to LAT,LON          destination (omit for nearest open shelter)\n"
         << "  --algo dijkstra|astar|both   (route; default both)\n"
         << "  --queries N           number of random queries (verify/bench)\n"
         << "  --seed S              random seed (verify/bench)\n"
         << "  --out FILE            output CSV (bench)\n"
         << "  --flood LAT,LON,KM    add a flood zone (repeatable)\n";
}

void fail(const string& message) {
    cerr << message << '\n';
    exit(1);
}

Options parseArgs(int argc, char** argv) {
    Options o;
    if (argc < 2) {
        usage();
        exit(1);
    }
    o.command = argv[1];
    for (int i = 2; i < argc; i += 2) {
        const string a = argv[i];
        if (i + 1 >= argc) fail("missing value for " + a);
        const string value = argv[i + 1];
        if (a == "--data") {
            o.dataDir = value;
        } else if (a == "--from") {
            parseLatLon(value, o.fromLat, o.fromLon);
            o.hasFrom = true;
        } else if (a == "--to") {
            parseLatLon(value, o.toLat, o.toLon);
            o.hasTo = true;
        } else if (a == "--algo") {
            o.algo = value;
            if (o.algo != "dijkstra" && o.algo != "astar" && o.algo != "both")
                fail("--algo must be dijkstra, astar or both");
        } else if (a == "--queries") {
            o.queries = stoi(value);
            if (o.queries <= 0) fail("--queries must be positive");
        } else if (a == "--seed") {
            o.seed = static_cast<unsigned>(stoul(value));
        } else if (a == "--out") {
            o.out = value;
        } else if (a == "--flood") {
            const size_t comma = value.rfind(',');
            FloodZone z;
            if (comma == string::npos) fail("--flood needs LAT,LON,KM");
            parseLatLon(value.substr(0, comma), z.lat, z.lon);
            z.radiusKm = stod(value.substr(comma + 1));
            o.floods.push_back(z);
        } else {
            cerr << "unknown option " << a << '\n';
            usage();
            exit(1);
        }
    }
    return o;
}

double median(vector<double> values) {
    sort(values.begin(), values.end());
    const size_t n = values.size();
    if (n % 2 == 1) return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

void printResult(Engine& e, const SearchResult& r, Algorithm algo) {
    cout << fixed << setprecision(2) << algorithmName(algo);
    if (!r.found) {
        cout << " no reachable goal (settled " << r.stats.settled << " nodes, "
             << r.stats.millis << " ms)\n";
        return;
    }
    cout << " length " << r.distance << " units (" << e.pathKm(r.path)
         << " km by road), " << r.path.size() << " intersections on path, settled "
         << r.stats.settled << ", relaxed " << r.stats.relaxed << ", "
         << r.stats.millis << " ms\n";
}

int cmdRoute(Engine& e, const Options& o) {
    if (!o.hasFrom) fail("route needs --from LAT,LON");
    const uint32_t src = e.scenario().snap(o.fromLon, o.fromLat);
    cout << fixed << setprecision(5) << "Origin snapped to intersection " << src + 1
         << " (" << e.graph().lat(src) << ", " << e.graph().lon(src) << ")\n";
    vector<Algorithm> algos;
    if (o.algo != "astar") algos.push_back(Algorithm::Dijkstra);
    if (o.algo != "dijkstra") algos.push_back(Algorithm::AStar);
    for (Algorithm a : algos) {
        SearchResult r;
        if (o.hasTo)
            r = e.router().route(a, src, e.scenario().snap(o.toLon, o.toLat));
        else
            r = e.router().routeToNearest(a, src, e.scenario().isGoal(), e.scenario().goalTree());
        printResult(e, r, a);
        if (r.found && !o.hasTo) {
            const int si = e.scenario().shelterAtNode(r.target);
            const Shelter& s = e.scenario().shelters()[si];
            const string cap = s.capacity > 0 ? to_string(s.capacity) : "not listed";
            cout << "  nearest open shelter: " << s.name << " (" << s.county
                 << " County), capacity " << cap << '\n';
        }
    }
    return 0;
}

int cmdVerify(Engine& e, const Options& o) {
    const int n = o.queries > 0 ? o.queries : 1000;
    mt19937 rng(o.seed);
    uniform_int_distribution<uint32_t> pick(0, e.graph().numNodes() - 1);
    int mismatchesNearest = 0, mismatchesPair = 0, foundNearest = 0, foundPair = 0;
    for (int i = 0; i < n; ++i) {
        const uint32_t src = pick(rng);
        const SearchResult d = e.router().routeToNearest(Algorithm::Dijkstra, src, e.scenario().isGoal(),
                                                        e.scenario().goalTree());
        const SearchResult a = e.router().routeToNearest(Algorithm::AStar, src, e.scenario().isGoal(),
                                                        e.scenario().goalTree());
        foundNearest += d.found ? 1 : 0;
        if (d.found != a.found || (d.found && d.distance != a.distance)) ++mismatchesNearest;
    }
    for (int i = 0; i < n; ++i) {
        const uint32_t src = pick(rng), dst = pick(rng);
        const SearchResult d = e.router().route(Algorithm::Dijkstra, src, dst);
        const SearchResult a = e.router().route(Algorithm::AStar, src, dst);
        foundPair += d.found ? 1 : 0;
        if (d.found != a.found || (d.found && d.distance != a.distance)) ++mismatchesPair;
    }
    cout << "Nearest-shelter queries: " << n << " origins, " << foundNearest
         << " reached a shelter, " << mismatchesNearest << " mismatches\n"
         << "Single-destination queries: " << n << " pairs, " << foundPair
         << " reachable, " << mismatchesPair << " mismatches\n";
    const bool ok = mismatchesNearest == 0 && mismatchesPair == 0;
    cout << (ok ? "PASS: Dijkstra and A* returned identical distances on every query\n"
                : "FAIL: the algorithms disagree\n");
    return ok ? 0 : 1;
}

int cmdBench(Engine& e, const Options& o) {
    const int n = o.queries > 0 ? o.queries : 500;
    const string outPath = o.out.empty() ? "benchmark.csv" : o.out;
    ofstream out(outPath);
    if (!out) fail("cannot write " + outPath);
    out << "query,type,algo,source,target,found,distance,settled,relaxed,ms\n";

    mt19937 rng(o.seed);
    uniform_int_distribution<uint32_t> pick(0, e.graph().numNodes() - 1);
    const string kinds[2] = {"nearest", "single"};
    const Algorithm algos[2] = {Algorithm::Dijkstra, Algorithm::AStar};
    vector<double> times[2][2], settled[2][2];
    int mismatches = 0;

    for (int type = 0; type < 2; ++type) {
        for (int q = 0; q < n; ++q) {
            const uint32_t src = pick(rng);
            uint32_t dst = 0;
            if (type == 1) dst = pick(rng);
            SearchResult results[2];
            for (int algo = 0; algo < 2; ++algo) {
                if (type == 0)
                    results[algo] = e.router().routeToNearest(
                        algos[algo], src, e.scenario().isGoal(), e.scenario().goalTree());
                else
                    results[algo] = e.router().route(algos[algo], src, dst);
            }
            const SearchResult& d = results[0];
            const SearchResult& a = results[1];
            if (d.found != a.found || (d.found && d.distance != a.distance)) ++mismatches;
            for (int algo = 0; algo < 2; ++algo) {
                const SearchResult& r = results[algo];
                out << q << ',' << kinds[type] << ',' << algorithmName(algos[algo]) << ','
                    << r.source + 1 << ',' << (r.found ? r.target + 1 : 0) << ',' << r.found
                    << ',' << r.distance << ',' << r.stats.settled << ',' << r.stats.relaxed
                    << ',' << r.stats.millis << '\n';
                times[type][algo].push_back(r.stats.millis);
                settled[type][algo].push_back(static_cast<double>(r.stats.settled));
            }
        }
    }

    cout << fixed << setprecision(3);
    for (int type = 0; type < 2; ++type) {
        for (int algo = 0; algo < 2; ++algo) {
            cout << kinds[type] << " / " << algorithmName(algos[algo])
                 << ": median " << median(times[type][algo]) << " ms, "
                 << setprecision(1) << median(settled[type][algo]) << " nodes settled\n"
                 << setprecision(3);
        }
    }
    cout << "Mismatches between Dijkstra and A*: " << mismatches << "\nWrote " << outPath << '\n';
    return mismatches == 0 ? 0 : 1;
}

}

int main(int argc, char** argv) {
    try {
        const Options o = parseArgs(argc, argv);
        Engine engine(o.dataDir);
        for (const FloodZone& z : o.floods) {
            engine.scenario().addFloodZone(z);
            cout << "Flood zone at (" << z.lat << ", " << z.lon << "), r = " << z.radiusKm
                 << " km: " << engine.graph().numClosed() << " arcs closed, "
                 << engine.scenario().numOpen() << " shelters open\n";
        }
        if (o.command == "route") return cmdRoute(engine, o);
        if (o.command == "verify") return cmdVerify(engine, o);
        if (o.command == "bench") return cmdBench(engine, o);
        usage();
        return 1;
    } catch (const exception& ex) {
        cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
