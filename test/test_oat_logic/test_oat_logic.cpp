#include <unity.h>
#include <oat_logic.h>

// 0.7 F is comfortably more than one 0.45 F quantisation step, so a single
// step of sensor noise can never move the display.
static const float HYST = 0.7f;

void test_first_reading_shows_immediately(void) {
  TEST_ASSERT_EQUAL_INT32(72, displayedOAT(72.4f, OAT_DISPLAY_UNSET, HYST));
  TEST_ASSERT_EQUAL_INT32(73, displayedOAT(72.6f, OAT_DISPLAY_UNSET, HYST));
}

// The bug: a reading dithering across x.5 used to alternate the display.
void test_boundary_dither_holds_steady(void) {
  long shown = 72;
  shown = displayedOAT(72.4f, shown, HYST); TEST_ASSERT_EQUAL_INT32(72, shown);
  shown = displayedOAT(72.6f, shown, HYST); TEST_ASSERT_EQUAL_INT32(72, shown);
  shown = displayedOAT(72.4f, shown, HYST); TEST_ASSERT_EQUAL_INT32(72, shown);
  shown = displayedOAT(72.6f, shown, HYST); TEST_ASSERT_EQUAL_INT32(72, shown);
}

void test_real_rise_still_moves_the_display(void) {
  long shown = 72;
  shown = displayedOAT(72.8f, shown, HYST);
  TEST_ASSERT_EQUAL_INT32(73, shown);
}

void test_real_fall_still_moves_the_display(void) {
  long shown = 72;
  shown = displayedOAT(71.2f, shown, HYST);
  TEST_ASSERT_EQUAL_INT32(71, shown);
}

// Once it has moved up, small dips must not drag it straight back.
void test_no_flip_back_after_a_move(void) {
  long shown = displayedOAT(72.8f, 72, HYST);
  TEST_ASSERT_EQUAL_INT32(73, shown);
  shown = displayedOAT(72.6f, shown, HYST);
  TEST_ASSERT_EQUAL_INT32(73, shown);
  shown = displayedOAT(72.4f, shown, HYST);
  TEST_ASSERT_EQUAL_INT32(73, shown);
}

// One 0.45 F quantisation step either way is never enough on its own.
void test_single_quantisation_step_is_ignored(void) {
  TEST_ASSERT_EQUAL_INT32(72, displayedOAT(72.0f + 0.45f, 72, HYST));
  TEST_ASSERT_EQUAL_INT32(72, displayedOAT(72.0f - 0.45f, 72, HYST));
}

void test_large_jump_tracks_immediately(void) {
  TEST_ASSERT_EQUAL_INT32(95, displayedOAT(95.1f, 72, HYST));
  TEST_ASSERT_EQUAL_INT32(-4, displayedOAT(-4.2f, 20, HYST));
}

void test_negative_temperatures(void) {
  long shown = -10;
  shown = displayedOAT(-10.4f, shown, HYST); TEST_ASSERT_EQUAL_INT32(-10, shown);
  shown = displayedOAT(-10.9f, shown, HYST); TEST_ASSERT_EQUAL_INT32(-11, shown);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_reading_shows_immediately);
  RUN_TEST(test_boundary_dither_holds_steady);
  RUN_TEST(test_real_rise_still_moves_the_display);
  RUN_TEST(test_real_fall_still_moves_the_display);
  RUN_TEST(test_no_flip_back_after_a_move);
  RUN_TEST(test_single_quantisation_step_is_ignored);
  RUN_TEST(test_large_jump_tracks_immediately);
  RUN_TEST(test_negative_temperatures);
  return UNITY_END();
}
