#!/usr/bin/env python3
"""Prepare offline city labels from an explicitly supplied 2010 Census ZIP."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import zipfile

SOURCE_HASH = "db63b4ef027f2118221c057b9e4911a4bbda09975bdc4aac7d0c0ba286e04c76"
ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets/maps/city-context-2010.json"


def prepare(path):
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SOURCE_HASH:
        raise ValueError("Source differs from the reviewed 2010 Census Places Gazetteer")
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        records = archive.read("Gaz_places_national.txt").decode("cp1252")
    best = {}
    for row in csv.DictReader(io.StringIO(records), delimiter="\t"):
        state = row["USPS"]
        if state == "PR":
            continue
        rank = (-int(row["POP10"]), row["GEOID"])
        if state not in best or rank < (-int(best[state]["POP10"]), best[state]["GEOID"]):
            best[state] = row
    if len(best) != 51:
        raise ValueError("Expected fifty states and the District of Columbia")
    aliases = {
        "Urban Honolulu CDP": "Honolulu",
        "Louisville/Jefferson County metro government (balance)": "Louisville",
        "Indianapolis city (balance)": "Indianapolis",
        "Boise City city": "Boise",
    }
    cities = {}
    for state, row in sorted(best.items()):
        name = row["NAME"]
        display = aliases.get(name, name)
        for suffix in (" city", " town", " village", " municipality", " borough", " CDP"):
            if display.endswith(suffix):
                display = display[:-len(suffix)]
                break
        cities["US-" + state] = {"name": display, "census_name": name,
                                  "geoid": row["GEOID"], "population": int(row["POP10"])}
    return json.dumps({"format": 1, "year": 2010, "cities": cities}, indent=2) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gazetteer", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = prepare(args.gazetteer)
    if args.check:
        if OUTPUT.read_text() != result:
            raise SystemExit("City context differs from the reviewed source")
        print("51 offline city labels match the reviewed 2010 Census source")
    else:
        OUTPUT.write_text(result)
