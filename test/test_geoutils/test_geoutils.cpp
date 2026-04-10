#include <unity.h>
#include "utils/GeoUtils.h"

void setUp() {}
void tearDown() {}

void test_distance_portland_to_boston() {
    // Portland ME → Boston MA ≈ 85.6 nm (Haversine result for these coordinates)
    float d = GeoUtils::distanceNm(43.6591f, -70.2568f, 42.3601f, -71.0589f);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 85.6f, d);
}

void test_distance_zero() {
    float d = GeoUtils::distanceNm(43.0f, -70.0f, 43.0f, -70.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, d);
}

void test_bearing_east() {
    // (0,0) → (0,1) = 90° east
    float b = GeoUtils::bearingDeg(0.0f, 0.0f, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 90.0f, b);
}

void test_bearing_north() {
    float b = GeoUtils::bearingDeg(0.0f, 0.0f, 1.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, b);
}

void test_bearing_south() {
    float b = GeoUtils::bearingDeg(1.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 180.0f, b);
}

void test_cardinal_N() {
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(0.0f));
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(359.0f));
    TEST_ASSERT_EQUAL_STRING("N", GeoUtils::cardinalDir(22.0f));
}

void test_cardinal_NE() {
    TEST_ASSERT_EQUAL_STRING("NE", GeoUtils::cardinalDir(47.0f));
}

void test_cardinal_E() {
    TEST_ASSERT_EQUAL_STRING("E", GeoUtils::cardinalDir(90.0f));
}

void test_blip_at_center() {
    // 0 distance → blip at center (95, 95)
    auto pos = GeoUtils::blipPosition(0.0f, 0.0f, 150.0f, 95, 6);
    TEST_ASSERT_EQUAL(95, pos.x);
    TEST_ASSERT_EQUAL(95, pos.y);
}

void test_blip_north_max_range() {
    // Due north, exactly max range, no margin → top-center (95, 0)
    auto pos = GeoUtils::blipPosition(150.0f, 0.0f, 150.0f, 95, 0);
    TEST_ASSERT_EQUAL(95, pos.x);
    TEST_ASSERT_INT_WITHIN(1, 0, pos.y);
}

void test_blip_east_half_range() {
    // Due east, half range → right-center (95 + 0.5*95, 95) ≈ (142, 95)
    auto pos = GeoUtils::blipPosition(75.0f, 90.0f, 150.0f, 95, 0);
    TEST_ASSERT_INT_WITHIN(2, 142, pos.x);
    TEST_ASSERT_INT_WITHIN(2, 95,  pos.y);
}

void test_blip_clamped_at_max_range() {
    // Beyond max range → clamped to radius - margin
    auto pos1 = GeoUtils::blipPosition(150.0f, 0.0f, 150.0f, 95, 6);
    auto pos2 = GeoUtils::blipPosition(300.0f, 0.0f, 150.0f, 95, 6);
    TEST_ASSERT_EQUAL(pos1.x, pos2.x);
    TEST_ASSERT_EQUAL(pos1.y, pos2.y);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_distance_portland_to_boston);
    RUN_TEST(test_distance_zero);
    RUN_TEST(test_bearing_east);
    RUN_TEST(test_bearing_north);
    RUN_TEST(test_bearing_south);
    RUN_TEST(test_cardinal_N);
    RUN_TEST(test_cardinal_NE);
    RUN_TEST(test_cardinal_E);
    RUN_TEST(test_blip_at_center);
    RUN_TEST(test_blip_north_max_range);
    RUN_TEST(test_blip_east_half_range);
    RUN_TEST(test_blip_clamped_at_max_range);
    return UNITY_END();
}
