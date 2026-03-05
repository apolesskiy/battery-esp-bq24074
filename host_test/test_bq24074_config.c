/**
 * @file test_bq24074_config.c
 * @brief Host-based tests for BQ24074 driver config and data structures.
 *
 * Tests config defaults, event enum values, state struct layout, and
 * API contracts that don't require hardware. Full integration tests
 * require on-device execution with a BQ24074-connected board.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bq24074_driver.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(fn) do { \
    printf("  Running %s... ", #fn); \
    fn(); \
    printf("PASS\n"); \
    tests_passed++; \
  } while (0)

// ============================================================================
// Config Default Macro Tests
// ============================================================================

static void test_config_default_values(void) {
  bq24074_config_t cfg = BQ24074_CONFIG_DEFAULT();
  assert(cfg.gpio_pgood == GPIO_NUM_NC);
  assert(cfg.gpio_pchg == GPIO_NUM_NC);
  assert(cfg.gpio_bat_lvl == GPIO_NUM_NC);
  assert(cfg.update_period_ms == 10000);
  assert(cfg.task_stack_size == 3072);
  assert(cfg.task_priority == 3);
}

static void test_config_default_battery_config(void) {
  bq24074_config_t cfg = BQ24074_CONFIG_DEFAULT();
  assert(cfg.battery_config.chemistry == BATTERY_CHEM_LIION);
  assert(cfg.battery_config.adc_max_mv == 0);
  assert(cfg.battery_config.divider_r1_kohm == 0);
  assert(cfg.battery_config.divider_r2_kohm == 0);
  assert(cfg.battery_config.custom_curve == NULL);
  assert(cfg.battery_config.custom_curve_len == 0);
}

static void test_config_custom_overrides(void) {
  bq24074_config_t cfg = BQ24074_CONFIG_DEFAULT();
  cfg.gpio_pgood = GPIO_NUM_1;
  cfg.gpio_pchg = GPIO_NUM_2;
  cfg.gpio_bat_lvl = GPIO_NUM_0;
  cfg.update_period_ms = 5000;
  cfg.battery_config.adc_max_mv = 3300;
  cfg.battery_config.divider_r1_kohm = 470;
  cfg.battery_config.divider_r2_kohm = 1000;

  assert(cfg.gpio_pgood == GPIO_NUM_1);
  assert(cfg.gpio_pchg == GPIO_NUM_2);
  assert(cfg.update_period_ms == 5000);
  assert(cfg.battery_config.divider_r1_kohm == 470);
}

// ============================================================================
// State Struct Tests
// ============================================================================

static void test_state_zero_init(void) {
  bq24074_state_t state = {0};
  assert(state.voltage_mv == 0);
  assert(state.percent == 0);
  assert(state.charging == false);
  assert(state.power_connected == false);
}

static void test_state_field_assignment(void) {
  bq24074_state_t state = {0};
  state.voltage_mv = 3700;
  state.percent = 50;
  state.charging = true;
  state.power_connected = true;

  assert(state.voltage_mv == 3700);
  assert(state.percent == 50);
  assert(state.charging);
  assert(state.power_connected);
}

// ============================================================================
// Event Enum Tests
// ============================================================================

static void test_event_enum_values_are_distinct(void) {
  /* All event values must be distinct for correct dispatch */
  bq24074_event_t events[] = {
      BQ24074_EVENT_VOLTAGE_UPDATE,
      BQ24074_EVENT_CHARGE_STARTED,
      BQ24074_EVENT_CHARGE_STOPPED,
      BQ24074_EVENT_POWER_CONNECTED,
      BQ24074_EVENT_POWER_DISCONNECTED,
  };

  int i, j;
  for (i = 0; i < 5; i++) {
    for (j = i + 1; j < 5; j++) {
      assert(events[i] != events[j]);
    }
  }
}

// ============================================================================
// Callback Type Tests
// ============================================================================

static bool callback_invoked = false;
static bq24074_event_t last_event;
static void *last_ctx = NULL;

static void test_callback(bq24074_event_t event, void *user_ctx) {
  callback_invoked = true;
  last_event = event;
  last_ctx = user_ctx;
}

static void test_callback_function_type(void) {
  /* Verify callback can be stored and invoked */
  bq24074_callback_t cb = test_callback;
  int ctx_data = 42;

  callback_invoked = false;
  cb(BQ24074_EVENT_CHARGE_STARTED, &ctx_data);

  assert(callback_invoked);
  assert(last_event == BQ24074_EVENT_CHARGE_STARTED);
  assert(last_ctx == &ctx_data);
}

int main(void) {
  printf("=== bq24074 config tests ===\n");
  RUN_TEST(test_config_default_values);
  RUN_TEST(test_config_default_battery_config);
  RUN_TEST(test_config_custom_overrides);
  RUN_TEST(test_state_zero_init);
  RUN_TEST(test_state_field_assignment);
  RUN_TEST(test_event_enum_values_are_distinct);
  RUN_TEST(test_callback_function_type);
  printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed;
}
