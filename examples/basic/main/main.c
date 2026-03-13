/**
 * @file basic_example.c
 * @brief Basic usage of battery_esp_bq24074 with an ESP32-S3.
 *
 * Initializes the BQ24074 driver and prints battery state
 * on each voltage update.
 */

#include <stdio.h>
#include "bq24074_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void battery_event_handler(bq24074_event_t event, void *ctx) {
  bq24074_handle_t handle = (bq24074_handle_t)ctx;
  bq24074_state_t state;
  bq24074_get_state(handle, &state);

  switch (event) {
    case BQ24074_EVENT_VOLTAGE_UPDATE:
      printf("Battery: %d mV (%d%%)\n", state.voltage_mv, state.percent);
      break;
    case BQ24074_EVENT_CHARGE_STARTED:
      printf("Charging started\n");
      break;
    case BQ24074_EVENT_CHARGE_STOPPED:
      printf("Charging stopped\n");
      break;
    case BQ24074_EVENT_POWER_CONNECTED:
      printf("Power connected\n");
      break;
    case BQ24074_EVENT_POWER_DISCONNECTED:
      printf("Power disconnected\n");
      break;
  }
}

void app_main(void) {
  bq24074_config_t config = BQ24074_CONFIG_DEFAULT();
  config.gpio_pgood = GPIO_NUM_4;
  config.gpio_pchg = GPIO_NUM_5;
  config.gpio_bat_lvl = GPIO_NUM_6;
  config.battery_config.chemistry = BATTERY_CHEM_LIION;
  config.battery_config.adc_max_mv = 3300;
  config.battery_config.divider_r1_kohm = 470;
  config.battery_config.divider_r2_kohm = 1000;
  config.battery_config.voltage_min_mv = 3000;
  config.battery_config.voltage_max_mv = 4200;
  config.update_period_ms = 5000;

  bq24074_handle_t handle;
  ESP_ERROR_CHECK(bq24074_init(&config, &handle));
  bq24074_set_callback(handle, battery_event_handler, handle);
  ESP_ERROR_CHECK(bq24074_start(handle));

  // Main loop keeps the task alive
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
