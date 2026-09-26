#include <unity.h>
#include <fuel_logic.h>

void test_unknown_before_any_frame(void) {
  TEST_ASSERT_EQUAL_INT(FUEL_UNKNOWN, fuelPercent(-1, FUEL_RAW_FULL));
}

// The observed full-tank reading must display as 100%, not 80%.
void test_observed_full_reads_100(void) {
  TEST_ASSERT_EQUAL_INT(100, fuelPercent(FUEL_RAW_FULL, FUEL_RAW_FULL));
}

void test_empty_reads_zero(void) {
  TEST_ASSERT_EQUAL_INT(0, fuelPercent(0, FUEL_RAW_FULL));
}

void test_half_tank(void) {
  TEST_ASSERT_EQUAL_INT(50, fuelPercent(58, FUEL_RAW_FULL));   // 14.5 gal
}

// The prediction that decides whether this decode is real: a quarter tank on a
// 36 gal tank is ~9 gal, so gallons x4 should read ~36.
void test_quarter_tank_prediction(void) {
  TEST_ASSERT_INT_WITHIN(2, 31, fuelPercent(36, FUEL_RAW_FULL));
}

// Overfill past the observed full point clamps rather than showing 105%.
void test_above_full_clamps(void) {
  TEST_ASSERT_EQUAL_INT(100, fuelPercent(130, FUEL_RAW_FULL));
}

void test_rounds_to_nearest(void) {
  TEST_ASSERT_EQUAL_INT(1, fuelPercent(1, FUEL_RAW_FULL));    // 0.86% -> 1
  TEST_ASSERT_EQUAL_INT(99, fuelPercent(115, FUEL_RAW_FULL));
}

void test_bad_calibration_is_not_a_divide_by_zero(void) {
  TEST_ASSERT_EQUAL_INT(FUEL_UNKNOWN, fuelPercent(100, 0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_unknown_before_any_frame);
  RUN_TEST(test_observed_full_reads_100);
  RUN_TEST(test_empty_reads_zero);
  RUN_TEST(test_half_tank);
  RUN_TEST(test_quarter_tank_prediction);
  RUN_TEST(test_above_full_clamps);
  RUN_TEST(test_rounds_to_nearest);
  RUN_TEST(test_bad_calibration_is_not_a_divide_by_zero);
  return UNITY_END();
}
