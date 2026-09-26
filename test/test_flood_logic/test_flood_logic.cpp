#include <unity.h>
#include <flood_logic.h>

static const unsigned long ARM = 2500;
static const unsigned long STALE = 3000;

// Beams on since t=0, a frame just arrived at `now`.
static bool armedAt(unsigned long now, unsigned long since) {
  return shouldLight(FLOOD_ARMED, true, now, since, now, ARM, STALE);
}

void test_off_never_lights(void) {
  TEST_ASSERT_FALSE(shouldLight(FLOOD_OFF, true, 10000, 0, 10000, ARM, STALE));
}

void test_on_lights_without_beams(void) {
  TEST_ASSERT_TRUE(shouldLight(FLOOD_ON, false, 10000, 0, 0, ARM, STALE));
}

void test_armed_lights_after_full_delay(void) {
  TEST_ASSERT_TRUE(armedAt(ARM, 0));
}

// The safety case: a flash must never light the bar. Measured stalk toggles on
// this truck ran 1.0-1.7 s, all comfortably under the 2500 ms guard.
void test_armed_rejects_a_flash(void) {
  TEST_ASSERT_FALSE(armedAt(1000, 0));
  TEST_ASSERT_FALSE(armedAt(1700, 0));
  TEST_ASSERT_FALSE(armedAt(ARM - 1, 0));
}

void test_armed_dark_when_beams_off(void) {
  TEST_ASSERT_FALSE(shouldLight(FLOOD_ARMED, false, 10000, 0, 10000, ARM, STALE));
}

// A quiet bus must drop the bar rather than hold the last known state.
void test_armed_dark_when_bus_stale(void) {
  TEST_ASSERT_TRUE(shouldLight(FLOOD_ARMED, true, 10000, 0, 10000 - (STALE - 1), ARM, STALE));
  TEST_ASSERT_FALSE(shouldLight(FLOOD_ARMED, true, 10000, 0, 10000 - STALE, ARM, STALE));
}

// After a stale gap the caller clears highBeamOn, so the next frame restarts
// the arming delay instead of the bar snapping straight back on.
void test_armed_dark_immediately_after_stale_recovery(void) {
  TEST_ASSERT_FALSE(shouldLight(FLOOD_ARMED, false, 14000, 0, 14000, ARM, STALE));
}

void test_millis_rollover(void) {
  unsigned long near = (unsigned long)-1000;  // 1 s before wrap
  TEST_ASSERT_TRUE(shouldLight(FLOOD_ARMED, true, near + ARM, near, near + ARM, ARM, STALE));
  TEST_ASSERT_FALSE(shouldLight(FLOOD_ARMED, true, near + 500, near, near + 500, ARM, STALE));
}

void test_mode_cycle_wraps(void) {
  FloodMode m = FLOOD_OFF;
  m = (FloodMode)((m + 1) % FLOOD_MODE_COUNT); TEST_ASSERT_EQUAL(FLOOD_ON, m);
  m = (FloodMode)((m + 1) % FLOOD_MODE_COUNT); TEST_ASSERT_EQUAL(FLOOD_ARMED, m);
  m = (FloodMode)((m + 1) % FLOOD_MODE_COUNT); TEST_ASSERT_EQUAL(FLOOD_OFF, m);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_off_never_lights);
  RUN_TEST(test_on_lights_without_beams);
  RUN_TEST(test_armed_lights_after_full_delay);
  RUN_TEST(test_armed_rejects_a_flash);
  RUN_TEST(test_armed_dark_when_beams_off);
  RUN_TEST(test_armed_dark_when_bus_stale);
  RUN_TEST(test_armed_dark_immediately_after_stale_recovery);
  RUN_TEST(test_millis_rollover);
  RUN_TEST(test_mode_cycle_wraps);
  return UNITY_END();
}
