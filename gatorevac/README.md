# GatorEvac

GatorEvac compares Dijkstra and A* on Florida shelter routes.

## Build and run

Run the commands from the folder containing `CMakeLists.txt`. You need g++ with C++17 support, CMake and Python.

Download the data and build:

```bash
python -m pip install ninja -r scripts/requirements.txt
python scripts/fetch_data.py
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, run:

```powershell
.\build\gatorevac-web.exe
```

On macOS, run `./build/gatorevac-web`.

Open `http://127.0.0.1:8080/` in Chrome or Edge. Keep the server running and use one tab for the demo.

## Using the app

- **Route:** click a starting point, then find the nearest open shelter or choose a destination.
- **Race:** replay both searches at the same number of nodes per second.
- **Floods and closures:** click shelters to toggle them, or add a flood zone to close roads. Reset restores the starting scenario.

The street background uses OpenFreeMap. If it is unavailable, the app draws the road graph.

## Command line

```bash
./build/gatorevac-cli verify --queries 1000 --seed 42
./build/gatorevac-cli bench --queries 500 --seed 7
```

On Windows, use `.\build\gatorevac-cli.exe`. `verify` compares route costs, and `bench` writes `benchmark.csv`.

## Data and libraries

The [DIMACS Florida road network](https://www.diag.uniroma1.it/challenge9/download.shtml) has 1,070,376 nodes and 2,712,798 directed arcs. The data script downloads distance arcs from [gpu_graph_algorithms](https://github.com/sengorajkumar/gpu_graph_algorithms) and coordinates from [BSM](https://github.com/lbwang95/BSM).

Shelters come from the [FEMA / American Red Cross HIFLD archive](https://source.coop/seerai/hifld). The filtered CSV has 2,514 facilities. INACTIVE shelters start closed, and facilities over 5 km from a road node are excluded.

HTTP uses [cpp-httplib v0.64.0](https://github.com/yhirose/cpp-httplib/tree/v0.64.0), bundled in `third_party/` with its MIT license. MapLibre GL JS and deck.gl are bundled in `web/vendor/` with their licenses.
