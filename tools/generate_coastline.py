#!/usr/bin/env python3
"""
Reference guide for regenerating src/data/CoastlinePortland.h.

This script is NOT a complete generator. It describes the steps needed
to regenerate the coastline data from Natural Earth 1:10m shapefiles.
Run this script to see the bounding box and simplification parameters.

Manual steps:
  1. pip install shapely fiona numpy
  2. Download ne_10m_land.shp from https://www.naturalearthdata.com/downloads/10m-physical-vectors/
  3. Clip to the bounding box printed below
  4. Apply Douglas-Peucker simplification (tolerance ~0.001 deg)
  5. Extract exterior ring lat/lon pairs
  6. Write as GeoPoint array into src/data/CoastlinePortland.h
"""

HOME_LAT, HOME_LON = 43.661, -70.255
RANGE_LAT = 0.42   # ~25nm N/S
RANGE_LON = 0.55   # ~25nm E/W at 43.66°N (1° lon ≈ 45.8nm here)

print("Clip bounding box:")
print(f"  lon: {HOME_LON-RANGE_LON:.3f} to {HOME_LON+RANGE_LON:.3f}")
print(f"  lat: {HOME_LAT-RANGE_LAT:.3f} to {HOME_LAT+RANGE_LAT:.3f}")
print("Apply Douglas-Peucker tolerance: 0.001 deg (~180m)")
print("Expected output: ~80-120 points covering Saco Bay to Freeport")
