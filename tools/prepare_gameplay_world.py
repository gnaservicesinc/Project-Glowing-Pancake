#!/usr/bin/env python3
"""Rebuild the selected founder-world draft from an explicitly supplied Census ZIP.
No network, runtime generation or changes to finalized content are performed.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("gazetteer", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
raw_zip = args.gazetteer.read_bytes()
with zipfile.ZipFile(io.BytesIO(raw_zip)) as archive:
    member = archive.namelist()[0]
    raw = archive.read(member).decode("cp1252")
rows = [{k.strip(): v.strip() for k, v in row.items()}
        for row in csv.DictReader(io.StringIO(raw), delimiter="\t")]
counties = []
for row in rows:
    if row["USPS"] == "PR":
        continue
    population = int(row["POP10"])
    density = population / max(1, float(row["ALAND_SQMI"]))
    # These thresholds create game archetypes, not official classifications.
    character = "Urban" if density >= 500 else "Suburban" if density >= 100 else "Rural" if density >= 10 else "Wilderness"
    development = 7500 if character == "Urban" else 6500 if character == "Suburban" else 4500 if character == "Rural" else 2500
    connections = "Regional roads" if character in ("Rural", "Wilderness") else "Road network and business connections"
    if float(row["AWATER_SQMI"]) >= 50:
        connections += " · water access"
    if row["GEOID"] in ("06017", "06051", "08049", "49043", "56039"):
        character = "Resort region"
        connections += " · tourism"
    counties.append(dict(id=row["GEOID"], state="US-" + row["USPS"], name=row["NAME"],
                         population=max(1, population), character=character, development=development,
                         connections=connections, latitude=round(float(row["INTPTLAT"]) * 1000000),
                         longitude=round(float(row["INTPTLONG"]) * 1000000)))
counties.sort(key=lambda x: x["id"])
assert len(counties) == 3143
collection = json.loads((root / "data/runtime/first-playable.json").read_text())
scenario = next(d for d in collection["definitions"] if d["id"] == "scenario.founder_campaign")
scenario["counties"] = dict(
    source="US Census Bureau, 2010 National Counties Gazetteer; names, population, area and representative points.",
    assumptions="Game conditions, not a forecast. County populations begin at the 2010 Census baseline. Character and connections are simplified game archetypes. Monthly changes combine bounded local variation with domestic and global welfare trends.",
    monthly_jitter=35, development_step=3, population_step=2, regional_weight=3, global_weight=1,
    counties=counties)
scenario["management"]["people"] = dict(
    first_names=["Alex", "Jordan", "Taylor", "Morgan", "Avery", "Casey", "Riley", "Jamie", "Sam", "Cameron", "Robin", "Drew", "Lee", "Quinn", "Rowan", "Sage", "Emery", "Skyler", "Reese", "Finley", "Parker", "Hayden", "Charlie", "Kendall", "Marin", "Blair", "Ellis", "Remy"],
    last_names=["Smith", "Johnson", "Williams", "Brown", "Garcia", "Miller", "Davis", "Martinez", "Lee", "Wilson", "Anderson", "Thomas", "Taylor", "Moore", "Martin", "Chen", "Patel", "Kim", "Nguyen", "Rivera", "Harris", "Clark", "Lewis", "Robinson", "Walker", "Young", "Allen", "King"],
    maximum_hq=128, candidates=4, recruitment_days=30, onboarding_days=7,
    monthly_salary_cents=500000, signing_cost_cents=100000, base_skill=50,
    skill_jitter=15, pay_skill_weight=20, prestige_skill_weight=2, mission_skill_weight=8,
    maximum_evaluation_credit_bp=2000)
output = root / "data/drafts/gameplay-world.json"
output.write_text(json.dumps(collection, ensure_ascii=False, separators=(",", ":")) + "\n")
provenance = {
    "file": "data/drafts/gameplay-world.json",
    "county_source": "https://www2.census.gov/geo/docs/maps-data/data/gazetteer/Gaz_counties_national.zip",
    "source_page": "https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.2010.html",
    "source_zip_sha256": hashlib.sha256(raw_zip).hexdigest(),
    "source_member": member,
    "source_rights": "US Census Bureau US-government geographic data. Existing Census terms are retained at assets/maps/CENSUS_TERMS.txt.",
    "transform": "Exclude Puerto Rico; retain county identifiers/names/2010 populations; convert representative points to integer microdegrees; author bounded game archetypes and connection summaries from population density/water area with five tourism overrides. Not official socioeconomic classifications.",
    "names": "Original selection of common names. Uses the pooled first-name/surname approach inspected in WTPStory1/game/functions/get_name.rpy; no Ren'Py code, full source list or story content copied. Core C++ uses its own deterministic identity stream.",
    "art": "Workspace geometry and interface are original Qt Quick/Quick3D source. Reference screenshots were studied for navigation relationships only; no image assets copied."
}
(root / "data/drafts/gameplay-world.provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
print(f"Wrote {len(counties)} counties and recruitment rules to {output}; {output.stat().st_size} bytes")
