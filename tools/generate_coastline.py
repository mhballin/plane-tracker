#!/usr/bin/env python3
"""
Generate src/data/CoastlinePortland.h from Natural Earth 1:10m coastline data.
Requires: pip install shapely requests numpy

Usage: python tools/generate_coastline.py
Output: src/data/CoastlinePortland.h (overwrites existing file)
"""
import json, urllib.request, os

HOME_LAT, HOME_LON = 43.661, -70.255
RANGE_DEG = 0.42  # ~25nm in degrees

# Natural Earth 1:10m land polygons (GeoJSON, clipped to NE US)
# Download from: https://www.naturalearthdata.com/downloads/10m-physical-vectors/
# Or use the OpenStreetMap coastline extract from osmcoastline.
# This script shows the approach; run manually to regenerate.

print("Download ne_10m_land.geojson from Natural Earth, then clip to:")
print(f"  bbox: {HOME_LON-RANGE_DEG:.3f},{HOME_LAT-RANGE_DEG:.3f},"
      f"{HOME_LON+RANGE_DEG:.3f},{HOME_LAT+RANGE_DEG:.3f}")
print("Use Douglas-Peucker simplification (tolerance ~0.001 deg)")
print("Extract exterior ring points and write to src/data/CoastlinePortland.h")
