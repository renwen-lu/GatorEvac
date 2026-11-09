import os
import urllib.request

import numpy as np
import pandas as pd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
RAW = os.path.join(DATA, "raw")

BSM = "https://raw.githubusercontent.com/lbwang95/BSM/main/data/FLA"
GPU = "https://raw.githubusercontent.com/sengorajkumar/gpu_graph_algorithms/master/input"
SHELTERS = ("https://s3.us-west-2.amazonaws.com/us-west-2.opendata.source.coop/seerai/hifld/"
            "national-shelter-system-facilities/shelter-locations/shelter-locations.parquet/"
            "part-00000-tid-5197556120553778283-36940850-fedb-4e12-aff7-d26063f5efd5-741-1-c000.zstd.parquet")

def download(url, name):
    path = os.path.join(RAW, name)
    print("  downloading", name)
    urllib.request.urlretrieve(url, path)
    return path


def csv_array(path):
    with open(path) as f:
        return np.array([int(x) for x in f.read().split(",") if x.strip()], dtype=np.int64)


def prepare_roads():
    frm = csv_array(download(f"{GPU}/USA-road-d.FLA.gr_FROM.csv", "FLA_FROM.csv"))
    to = csv_array(download(f"{GPU}/USA-road-d.FLA.gr_TO.csv", "FLA_TO.csv"))
    w = csv_array(download(f"{GPU}/USA-road-d.FLA.gr_W.csv", "FLA_W.csv"))
    co = download(f"{BSM}/USA-road-d.FLA.co", "USA-road-d.FLA.co")

    assert len(frm) == len(to) == len(w)
    with open(co) as f:
        nodes = sum(1 for line in f if line.startswith("v "))

    out = os.path.join(DATA, "USA-road-d.FLA.gr")
    with open(out, "w") as f:
        f.write("c 9th DIMACS Implementation Challenge: Shortest Paths - USA-road-d.FLA (distance)\n")
        f.write("c rebuilt by scripts/fetch_data.py from github.com/sengorajkumar/gpu_graph_algorithms\n")
        f.write(f"p sp {nodes} {len(frm)}\n")
        f.writelines(f"a {a} {b} {c}\n" for a, b, c in zip(frm, to, w))
    with open(co) as src, open(os.path.join(DATA, "USA-road-d.FLA.co"), "w") as dst:
        dst.write(src.read())
    print("  wrote data/USA-road-d.FLA.gr and data/USA-road-d.FLA.co")


def prepare_shelters():
    df = pd.read_parquet(download(SHELTERS, "shelter-locations.parquet"))
    fl = df[df["state"].astype(str).str.upper() == "FL"]
    fl = fl[fl["latitude"].between(24.0, 31.1) & fl["longitude"].between(-87.7, -79.8)]
    keep = ["shelter_id", "shelter_name", "county_parish", "city", "latitude", "longitude",
            "evacuation_capacity"]
    fl = fl[keep].sort_values("shelter_id").drop_duplicates("shelter_id")
    for c in ["shelter_name", "county_parish", "city"]:
        fl[c] = fl[c].fillna("").astype(str).str.replace(",", " ").str.replace("\n", " ").str.strip()
    fl["evacuation_capacity"] = fl["evacuation_capacity"].fillna(0).astype(int)
    fl.to_csv(os.path.join(DATA, "shelters_fl.csv"), index=False)
    print(f"  wrote data/shelters_fl.csv ({len(fl)} shelters with coordinates inside Florida)")


def main():
    os.makedirs(RAW, exist_ok=True)
    print("Road network:")
    prepare_roads()
    print("Shelters:")
    prepare_shelters()
    print("Done. Delete data/raw/ to save space if you like.")


if __name__ == "__main__":
    main()
