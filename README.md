# GatorEvac

COP3530 Project 2 ¡¤ Group 88

GatorEvac compares Dijkstra and A* on Florida's road network. Use the map to find the nearest open shelter, choose a destination, or see how a flood zone changes the available routes.

## Build and run

You need Python with pip, CMake 3.16 or newer, and `g++` with C++17 support.

From the repository root, download the data and build the project:

```sh
cd gatorevac
python -m pip install ninja -r scripts/requirements.txt
python scripts/fetch_data.py
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake needs `g++` and `ninja` on your `PATH`. If your system uses `python3`, use it in the two Python commands above.

Stay in the `gatorevac` folder and start the server.

**Windows**

```cmd
.\build\gatorevac-web.exe
```

**macOS**

```sh
./build/gatorevac-web
```

Open [http://127.0.0.1:8080/](http://127.0.0.1:8080/) in your browser. Keep the server running while you use the app.

## Using the map

- **Route:** Click a starting point, then route to the nearest open shelter or pick a destination on the map.
- **Race:** Replay both searches side by side at the same number of nodes per second.
- **Floods and closures:** Add a flood zone to close roads or click a shelter to change its status. Use **Reset** to restore the starting scenario.

The street map uses OpenFreeMap. If the map background is unavailable, GatorEvac draws the road network directly.

## Command line

Run these commands from `gatorevac/`.

**Windows**

```cmd
.\build\gatorevac-cli.exe verify --queries 1000 --seed 42
.\build\gatorevac-cli.exe bench --queries 500 --seed 7
```

**macOS**

```sh
./build/gatorevac-cli verify --queries 1000 --seed 42
./build/gatorevac-cli bench --queries 500 --seed 7
```

`verify` checks whether the algorithms agree on reachability and route cost. `bench` writes a new `benchmark.csv` in the current folder. The original results used for Table 1 are in [GatorEvac_benchmark.csv](GatorEvac_benchmark.csv).

## Data and libraries

- **Road network:** The [DIMACS Florida road network](https://www.diag.uniroma1.it/challenge9/download.shtml) contains 1,070,376 nodes and 2,712,798 directed arcs. The download script gets road arcs from [gpu_graph_algorithms](https://github.com/sengorajkumar/gpu_graph_algorithms) and coordinates from [BSM](https://github.com/lbwang95/BSM).
- **Shelters:** The [FEMA and American Red Cross shelter data](https://source.coop/seerai/hifld) was filtered to 2,514 facilities and is included in [shelters_fl.csv](gatorevac/data/shelters_fl.csv). Shelters marked `INACTIVE` start closed. Shelters more than 5 km from a road node are excluded from routing.
- **Libraries:** The server uses cpp-httplib. The map uses MapLibre GL JS and deck.gl. Their licenses are included in [third_party](gatorevac/third_party) and [web/vendor](gatorevac/web/vendor).