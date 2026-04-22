// src/data/CoastlinePortland.h
// Simplified Maine coastline for Portland area radar display (~25nm radius).
// Traces the land/water boundary SW→NE. Land is to the west/northwest.
// Approximate — suitable for visual context, not navigation.
// To regenerate with higher fidelity, run tools/generate_coastline.py.
#pragma once

struct GeoPoint { float lat; float lon; };

static const GeoPoint COASTLINE_PORTLAND[] = {
    // Southern boundary — Saco / Scarborough area
    {43.498f, -70.450f},
    {43.510f, -70.388f},
    {43.525f, -70.365f},
    // Prouts Neck
    {43.537f, -70.338f},
    {43.551f, -70.323f},
    {43.556f, -70.302f},
    // Crescent Beach / Ocean Ave
    {43.559f, -70.265f},
    {43.561f, -70.234f},
    {43.565f, -70.200f},
    // Cape Elizabeth / Two Lights
    {43.570f, -70.193f},
    {43.581f, -70.193f},
    {43.607f, -70.200f},
    // Portland Head Light
    {43.623f, -70.207f},
    // Inner Casco Bay / Portland harbor
    {43.636f, -70.198f},
    {43.647f, -70.215f},
    {43.657f, -70.223f},
    // Eastern Promenade / Back Cove
    {43.662f, -70.232f},
    {43.668f, -70.237f},
    {43.673f, -70.239f},
    // East Deering / Martin's Point
    {43.681f, -70.234f},
    {43.694f, -70.228f},
    {43.707f, -70.220f},
    // Falmouth Foreside
    {43.720f, -70.218f},
    {43.729f, -70.214f},
    // Falmouth → Yarmouth
    {43.745f, -70.194f},
    {43.756f, -70.180f},
    {43.769f, -70.173f},
    // Yarmouth / Royal River
    {43.785f, -70.178f},
    {43.799f, -70.185f},
    // North toward Freeport (northern boundary)
    {43.814f, -70.192f},
    {43.840f, -70.175f},
    {43.870f, -70.155f},
};

static const int COASTLINE_PORTLAND_LEN =
    static_cast<int>(sizeof(COASTLINE_PORTLAND) / sizeof(COASTLINE_PORTLAND[0]));

// PWM airport position (Portland International Jetport)
constexpr float COASTLINE_PWM_LAT = 43.6462f;
constexpr float COASTLINE_PWM_LON = -70.3093f;
