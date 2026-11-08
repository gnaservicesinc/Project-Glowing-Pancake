#!/usr/bin/env python3
"""Prepare an offline display map from the retained public-domain Natural Earth source."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/maps/sources/ne_110m_admin_0_countries.geojson"
TARGET = ROOT / "assets/maps/world-countries.json"

def region(properties):
    code = properties["ADM0_A3"]
    if code == "USA": return "region.us"
    if code in ("CAN", "GRL"): return "region.canada"
    if code == "CHN": return "region.china"
    if code == "IND": return "region.india"
    if properties["CONTINENT"] == "Europe": return "region.europe"
    if properties["CONTINENT"] in ("North America", "South America"): return "region.latin_america"
    if properties["CONTINENT"] == "Africa" or properties["SUBREGION"] == "Western Asia": return "region.africa_middle_east"
    return "region.asia_pacific"

def prepare():
    rows = []
    for feature in json.loads(SOURCE.read_text())["features"]:
        p, g = feature["properties"], feature["geometry"]
        if p["CONTINENT"] == "Antarctica": continue
        polygons = g["coordinates"] if g["type"] == "MultiPolygon" else [g["coordinates"]]
        # Equirectangular display, integer hundredths of a degree. No economic source fields.
        rings = [[[round((lon + 180) * 100), round((90 - lat) * 100)] for lon, lat in poly[0]] for poly in polygons]
        rows.append({"id":p["ADM0_A3"], "name":p["NAME_LONG"], "region":region(p), "rings":rings})
    return json.dumps({"source_sha256":hashlib.sha256(SOURCE.read_bytes()).hexdigest(), "width":36000,"height":18000,
        "credit":"Made with Natural Earth. Public domain. Modern display boundaries; company outcomes use fictional game regions.",
        "countries":sorted(rows,key=lambda r:r["name"])},ensure_ascii=False,separators=(",", ":")) + "\n"

if __name__ == "__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--check",action="store_true");args=parser.parse_args()
    result=prepare()
    if args.check:
        if TARGET.read_text()!=result: raise SystemExit("World display geometry is not reproducible")
        print("World display geometry reproduced exactly")
    else:
        TARGET.write_text(result)
        print(f"Prepared {len(json.loads(result)['countries'])} country display features")
