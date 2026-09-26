#include <unity.h>
#include <touch_logic.h>

// Shared with the firmware, so a recalibration keeps these tests honest.
static const TouchCal CAL = touchCal();

// The FLOOD button, mirroring the layout defines in main.cpp
static const int BX = 220, BY = 10, BW = 90, BH = 80, MARGIN = 8;

static bool hitsButton(int rawX, int rawY) {
  return touchInRect(touchScreenX(rawX, CAL), touchScreenY(rawY, CAL),
                     BX, BY, BW, BH, MARGIN);
}

void test_axes_are_inverted(void) {
  TEST_ASSERT_EQUAL_INT(239, touchScreenY(CAL.minY, CAL));  // low raw  = bottom
  TEST_ASSERT_EQUAL_INT(0,   touchScreenY(CAL.maxY, CAL));  // high raw = top
  TEST_ASSERT_EQUAL_INT(319, touchScreenX(CAL.minX, CAL));
  TEST_ASSERT_EQUAL_INT(0,   touchScreenX(CAL.maxX, CAL));
}

void test_centre_maps_to_centre(void) {
  TEST_ASSERT_INT_WITHIN(3, 160, touchScreenX((CAL.minX + CAL.maxX) / 2, CAL));
  TEST_ASSERT_INT_WITHIN(3, 120, touchScreenY((CAL.minY + CAL.maxY) / 2, CAL));
}

void test_out_of_range_clamps(void) {
  TEST_ASSERT_EQUAL_INT(0,   touchScreenY(CAL.maxY + 500, CAL));
  TEST_ASSERT_EQUAL_INT(239, touchScreenY(CAL.minY - 500, CAL));
}

// The #9 regression: real presses on the FLOOD card must land inside it.
// With MAXY at 2830 these all mapped to sy=0 and missed by one pixel.
void test_real_button_presses_register(void) {
  TEST_ASSERT_TRUE(hitsButton(1134, 3055));
  TEST_ASSERT_TRUE(hitsButton(1074, 3151));
  TEST_ASSERT_TRUE(hitsButton(1262, 2921));
  TEST_ASSERT_TRUE(hitsButton(1174, 2983));
  TEST_ASSERT_TRUE(hitsButton(1138, 3119));
}

// A calibration range short at the top reproduces the bug, so this test would
// have caught it rather than an evening in the driveway.
void test_short_maxy_reproduces_the_bug(void) {
  const TouchCal bad = { 780, 3100, 800, 2830 };
  int sy = touchScreenY(3055, bad);
  TEST_ASSERT_EQUAL_INT(0, sy);
  TEST_ASSERT_FALSE(touchInRect(touchScreenX(1134, bad), sy,
                                BX, BY, BW, BH, MARGIN));
}

void test_presses_elsewhere_miss(void) {
  TEST_ASSERT_FALSE(hitsButton(CAL.minX + 100, CAL.minY + 100));  // bottom-left
  TEST_ASSERT_FALSE(hitsButton((CAL.minX + CAL.maxX) / 2,
                               (CAL.minY + CAL.maxY) / 2));       // centre
}

void test_degenerate_range_does_not_divide_by_zero(void) {
  const TouchCal flat = { 1000, 1000, 1000, 1000 };
  TEST_ASSERT_EQUAL_INT(0, touchScreenX(1000, flat));
  TEST_ASSERT_EQUAL_INT(0, touchScreenY(1000, flat));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_axes_are_inverted);
  RUN_TEST(test_centre_maps_to_centre);
  RUN_TEST(test_out_of_range_clamps);
  RUN_TEST(test_real_button_presses_register);
  RUN_TEST(test_short_maxy_reproduces_the_bug);
  RUN_TEST(test_presses_elsewhere_miss);
  RUN_TEST(test_degenerate_range_does_not_divide_by_zero);
  return UNITY_END();
}
