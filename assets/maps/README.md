# Offline domestic map

The 2024 U.S. Census Bureau 1:5,000,000 KML and its exact metadata are retained in `sources/`. The downloaded ZIP SHA-256 and source/license/credit record are in `../manifest.json`. The geometry is available for product/publication use with Census acknowledgement, subject to the retained visual-display restrictions in [CENSUS_TERMS.txt](CENSUS_TERMS.txt). No project GPL claim is made over this third-party map data.

Run `python3 tools/prepare_us_map.py --check` from the repository to reproduce the reviewed 51-jurisdiction integer geometry. Preparation is local and never downloads at build/runtime. The chart uses a fixed display layout, two-unit subpixel simplification, scaled Alaska/Hawaii insets and an enlarged DC selection marker. Original geographic data are used only for visual display; game economics, travel zones, costs and delays are independently authored approximations.

The runtime receives only `us-states-2024.json` and the offline terms/credit, through explicit resource entries. Original KML/metadata and the preparation script belong to the source checkout. The game provides a complete state list and keyboard equivalent; no map hotspot is needed for management.
