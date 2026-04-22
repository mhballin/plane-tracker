// src/utils/GeoUtils.h
// Pure-math geo utilities — no Arduino dependency, testable on native.
#pragma once
#include <cmath>
#include <cstdint>

namespace GeoUtils {

constexpr float EARTH_RADIUS_NM = 3440.065f;
constexpr float DEG_TO_RAD = static_cast<float>(3.14159265358979323846) / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / static_cast<float>(3.14159265358979323846);

/// Haversine great-circle distance in nautical miles.
inline float distanceNm(float lat1, float lon1, float lat2, float lon2) {
    float dlat = (lat2 - lat1) * DEG_TO_RAD;
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    float a = sinf(dlat / 2) * sinf(dlat / 2)
            + cosf(lat1 * DEG_TO_RAD) * cosf(lat2 * DEG_TO_RAD)
            * sinf(dlon / 2) * sinf(dlon / 2);
    return 2.0f * EARTH_RADIUS_NM * asinf(sqrtf(a));
}

/// Forward bearing in degrees [0, 360).
inline float bearingDeg(float lat1, float lon1, float lat2, float lon2) {
    float dlon = (lon2 - lon1) * DEG_TO_RAD;
    float y = sinf(dlon) * cosf(lat2 * DEG_TO_RAD);
    float x = cosf(lat1 * DEG_TO_RAD) * sinf(lat2 * DEG_TO_RAD)
            - sinf(lat1 * DEG_TO_RAD) * cosf(lat2 * DEG_TO_RAD) * cosf(dlon);
    float b = atan2f(y, x) * RAD_TO_DEG;
    return fmodf(b + 360.0f, 360.0f);
}

/// 8-point cardinal direction string from a bearing in degrees.
inline const char* cardinalDir(float deg) {
    static const char* dirs[8] = { "N","NE","E","SE","S","SW","W","NW" };
    int sector = static_cast<int>((deg + 22.5f) / 45.0f) % 8;
    return dirs[sector];
}

/// Pixel position of an aircraft blip on the radar circle.
/// circleRadius: half of circle width/height (e.g. 95 for a 190px circle).
/// blipMargin: keeps blip inside circle edge (default 6px for an 8px blip).
/// Returns pixel (x, y) relative to top-left of the radar circle container,
/// where (circleRadius, circleRadius) is the center.
struct BlipPos { int16_t x; int16_t y; };

inline BlipPos blipPosition(float distNm, float bearingDegVal,
                             float maxRangeNm, int16_t circleRadius,
                             int16_t blipMargin = 6) {
    float scale = (distNm < maxRangeNm) ? (distNm / maxRangeNm) : 1.0f;
    float r = scale * static_cast<float>(circleRadius - blipMargin);
    float rad = bearingDegVal * DEG_TO_RAD;
    return {
        static_cast<int16_t>(circleRadius + r * sinf(rad)),
        static_cast<int16_t>(circleRadius - r * cosf(rad))
    };
}

/// Project a geographic lat/lon point onto the radar circle.
/// Equivalent to distanceNm + bearingDeg + blipPosition with zero margin.
/// Use for rendering coastline points and fixed markers.
inline BlipPos latLonToRadarPx(float ptLat, float ptLon,
                                float centerLat, float centerLon,
                                float maxRangeNm, int16_t circleRadius) {
    float dist = distanceNm(centerLat, centerLon, ptLat, ptLon);
    float bear = bearingDeg(centerLat, centerLon, ptLat, ptLon);
    return blipPosition(dist, bear, maxRangeNm, circleRadius, 0);
}

} // namespace GeoUtils
