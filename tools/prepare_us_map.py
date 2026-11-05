#!/usr/bin/env python3
"""Prepare bounded display-only US state geometry from the retained Census KML.

No runtime or build-time network access. Run explicitly after reviewing a source
update; --check reproduces the checked-in geometry without changing files.
Business profiles and travel terms are authored separately, never measured from
this cartographic display data.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/maps/sources/cb_2024_us_state_5m.kml'
OUTPUT = ROOT / 'assets/maps/us-states-2024.json'
SOURCE_SHA = 'f8a4f15f338a61ab4f1491c48cf00ab20558908fc6073b6b4f871ad7320869a1'
STATES = frozenset('AL AK AZ AR CA CO CT DE DC FL GA HI ID IL IN IA KS KY LA ME MD MA MI MN MS MO MT NE NV NH NJ NM NY NC ND OH OK OR PA RI SC SD TN TX UT VT VA WA WV WI WY'.split())
NS = {'k': 'http://www.opengis.net/kml/2.2'}


def simplify(points: list[tuple[int, int]], tolerance: int = 2) -> list[list[int]]:
    """Reduce subpixel display segments; distances are canvas units only."""
    unique = [points[0]]
    for point in points[1:]:
        if point != unique[-1]:
            unique.append(point)
    if unique[-1] == unique[0]:
        unique.pop()
    if len(unique) < 3:
        return []

    def segment(part: list[tuple[int, int]]) -> list[tuple[int, int]]:
        if len(part) < 3:
            return part
        a, b = part[0], part[-1]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = dx * dx + dy * dy
        best, index = 0.0, 0
        for i, p in enumerate(part[1:-1], 1):
            projection = 0.0 if length == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length))
            distance = (p[0] - a[0] - projection * dx) ** 2 + (p[1] - a[1] - projection * dy) ** 2
            if distance > best:
                best, index = distance, i
        if best <= tolerance * tolerance:
            return [a, b]
        return segment(part[:index + 1])[:-1] + segment(part[index:])

    split = max(range(1, len(unique)), key=lambda i: (unique[i][0] - unique[0][0]) ** 2 + (unique[i][1] - unique[0][1]) ** 2)
    result = segment(unique[:split + 1])[:-1] + segment(unique[split:] + [unique[0]])[:-1]
    if len(result) < 3:
        result = unique
    return [[x, y] for x, y in result + [result[0]]]


def prepare(source: bytes) -> bytes:
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA:
        raise ValueError('Source differs from the reviewed 2024 Census KML')
    if len(source) > 8_000_000:
        raise ValueError('Display source exceeds its bound')
    root = ET.fromstring(source)
    records = []
    for mark in root.findall('.//k:Placemark', NS):
        fields = {row.attrib['name']: row.text for row in mark.findall('.//k:SimpleData', NS)}
        code = fields['STUSPS']
        if code in ('PR', 'AS', 'GU', 'MP', 'VI'):
            continue  # The domestic scope is the fifty states and DC.
        if code not in STATES:
            raise ValueError(f'Unexpected jurisdiction: {code}')
        rings = []
        for coordinates in mark.findall('.//k:LinearRing/k:coordinates', NS):
            ring = []
            for value in (coordinates.text or '').split():
                lon, lat, *_ = map(float, value.split(','))
                if code == 'AK' and lon > 0:
                    lon -= 360
                if not -190 <= lon <= -65 or not 15 <= lat <= 73:
                    raise ValueError('Coordinate outside the selected display extent')
                ring.append((lon, lat))
            if len(ring) >= 4:
                rings.append(ring)
        if not rings:
            raise ValueError(f'Missing polygon display data for {code}')
        records.append({'id': f'US-{code}', 'name': fields['NAME'], 'fips': fields['STATEFP'], 'rings': rings})
    if {r['id'][3:] for r in records} != STATES or len(records) != 51:
        raise ValueError('Expected exactly fifty states and DC')

    def fit(group, box, latitude):
        # Latitude/longitude layout, scaled only for a compact visual display.
        cosine = math.cos(math.radians(latitude))
        points = [(x * cosine, -y) for r in group for ring in r['rings'] for x, y in ring]
        left, right = min(p[0] for p in points), max(p[0] for p in points)
        top, bottom = min(p[1] for p in points), max(p[1] for p in points)
        x0, y0, width, height = box
        scale = min(width / (right - left), height / (bottom - top))
        xpad, ypad = (width - (right - left) * scale) / 2, (height - (bottom - top) * scale) / 2
        for record in group:
            transformed = []
            for ring in record['rings']:
                points = [(round(x0 + xpad + (x * cosine - left) * scale), round(y0 + ypad + (-y - top) * scale)) for x, y in ring]
                reduced = simplify(points)
                if reduced:
                    transformed.append(reduced)
            if not transformed:
                raise ValueError('Quantization erased a jurisdiction')
            record['rings'] = transformed
            points = [p for ring in transformed for p in ring]
            record['label'] = [(min(p[i] for p in points) + max(p[i] for p in points)) // 2 for i in (0, 1)]
            record['callout'] = record['id'] == 'US-DC'

    fit([r for r in records if r['id'] not in ('US-AK', 'US-HI')], (150, 100, 9700, 4750), 38)
    fit([r for r in records if r['id'] == 'US-AK'], (250, 4950, 3400, 1700), 55)
    fit([r for r in records if r['id'] == 'US-HI'], (4300, 5350, 2600, 1100), 20)
    records.sort(key=lambda r: r['name'])
    total = sum(len(ring) for r in records for ring in r['rings'])
    if total > 100_000:
        raise ValueError('Prepared display geometry exceeds its point bound')
    result = {'format': 1, 'asset_id': 'map.us_states_2024', 'source_year': 2024, 'width': 10000, 'height': 6800,
              'credit': 'U.S. Census Bureau, 2024 Cartographic Boundary Files, 1:5,000,000. Prepared for visual game display; Alaska and Hawaii are scaled insets; DC has an enlarged selection marker.',
              'source_url': 'https://www.census.gov/geographies/mapping-files/2024/geo/carto-boundary-file.html',
              'license': 'Free product/publication use with U.S. Census Bureau acknowledgement; display only. Retained terms: assets/maps/CENSUS_TERMS.txt.',
              'features': records}
    return (json.dumps(result, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    result = prepare(SOURCE.read_bytes())
    if args.check:
        if OUTPUT.read_bytes() != result:
            raise ValueError('Prepared map differs from the reproducible source transformation')
    else:
        OUTPUT.write_bytes(result)
    decoded = json.loads(result)
    print(f"51 jurisdictions; {sum(len(ring) for feature in decoded['features'] for ring in feature['rings'])} display points; {len(result)} bytes; SHA256 {hashlib.sha256(result).hexdigest()}")


if __name__ == '__main__':
    main()
