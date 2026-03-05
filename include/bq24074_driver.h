/*
 * SPDX-FileCopyrightText: 2026 Aleksey Polesskiy
 * SPDX-License-Identifier: MIT
 *
 * BQ24074 battery charger IC driver.
 *
 * Standalone ESP-IDF driver — reads battery voltage via ADC and
 * charge/power status via GPIOs. No application framework dependency.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "battery_charge_percent.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Battery state snapshot. */
typedef struct {
  uint16_t voltage_mv;   /**< Battery voltage in millivolts. */
  uint8_t percent;        /**< Estimated charge percentage (0-100). */
  bool charging;          /**< True when the BQ24074 is actively charging. */
  bool power_connected;   /**< True when external power is present. */
} bq24074_state_t;

/** Events emitted by the driver on state changes. */
typedef enum {
  BQ24074_EVENT_VOLTAGE_UPDATE,    /**< Periodic voltage reading updated. */
  BQ24074_EVENT_CHARGE_STARTED,    /**< Charging began. */
  BQ24074_EVENT_CHARGE_STOPPED,    /**< Charging ended. */
  BQ24074_EVENT_POWER_CONNECTED,   /**< External power connected. */
  BQ24074_EVENT_POWER_DISCONNECTED /**< External power removed. */
} bq24074_event_t;

/** Callback invoked from the driver's monitoring task on events. */
typedef void (*bq24074_callback_t)(bq24074_event_t event, void *user_ctx);

/** Driver configuration. */
typedef struct {
  gpio_num_t gpio_pgood;    /**< !PGOOD pin (active-low: power present). */
  gpio_num_t gpio_pchg;     /**< !PCHG pin  (active-low: charging). */
  gpio_num_t gpio_bat_lvl;  /**< Battery voltage ADC input pin. */
  battery_charge_percent_config_t battery_config; /**< Voltage-to-percent params. */
  uint32_t update_period_ms; /**< Polling period in ms (default: 10000). */
  uint32_t task_stack_size;  /**< Monitor task stack size (default: 3072). */
  UBaseType_t task_priority; /**< Monitor task priority (default: 3). */
} bq24074_config_t;

/** Default values for optional config fields. */
#define BQ24074_CONFIG_DEFAULT() { \
  .gpio_pgood = GPIO_NUM_NC, \
  .gpio_pchg = GPIO_NUM_NC, \
  .gpio_bat_lvl = GPIO_NUM_NC, \
  .battery_config = { \
    .chemistry = BATTERY_CHEM_LIION, \
    .adc_max_mv = 0, \
    .divider_r1_kohm = 0, \
    .divider_r2_kohm = 0, \
    .voltage_min_mv = 0, \
    .voltage_max_mv = 0, \
    .custom_curve = NULL, \
    .custom_curve_len = 0, \
  }, \
  .update_period_ms = 10000, \
  .task_stack_size = 3072, \
  .task_priority = 3, \
}

/** Opaque driver handle. */
typedef struct bq24074_driver *bq24074_handle_t;

/**
 * @brief Allocate and initialise a BQ24074 driver instance.
 *
 * Does not start the monitoring task — call bq24074_start() afterwards.
 *
 * @param[in]  config  Driver configuration (copied internally).
 * @param[out] out_handle  Receives the new driver handle.
 * @return ESP_OK on success.
 */
esp_err_t bq24074_init(const bq24074_config_t *config,
                       bq24074_handle_t *out_handle);

/**
 * @brief Stop the driver and free all resources.
 *
 * Safe to call on a stopped or never-started handle.
 *
 * @param handle  Driver handle (set to NULL after return).
 */
void bq24074_deinit(bq24074_handle_t handle);

/**
 * @brief Start the monitoring task and GPIO interrupts.
 *
 * @param handle  Driver handle.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already running.
 */
esp_err_t bq24074_start(bq24074_handle_t handle);

/**
 * @brief Stop the monitoring task and remove GPIO interrupts.
 *
 * @param handle  Driver handle.
 */
void bq24074_stop(bq24074_handle_t handle);

/**
 * @brief Get the latest battery state snapshot (lock-free).
 *
 * @param handle  Driver handle.
 * @param[out] out_state  Receives a copy of the current state.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if NULL.
 */
esp_err_t bq24074_get_state(bq24074_handle_t handle,
                            bq24074_state_t *out_state);

/**
 * @brief Register a callback for battery events.
 *
 * The callback is invoked from the driver's monitoring task context.
 *
 * @param handle    Driver handle.
 * @param callback  Function pointer (NULL to clear).
 * @param user_ctx  Opaque context passed to the callback.
 * @return ESP_OK on success.
 */
esp_err_t bq24074_set_callback(bq24074_handle_t handle,
                               bq24074_callback_t callback,
                               void *user_ctx);

/**
 * @brief Wake the monitoring task for an immediate reading.
 *
 * @param handle  Driver handle.
 * @return ESP_OK on success.
 */
esp_err_t bq24074_request_update(bq24074_handle_t handle);

#ifdef __cplusplus
}
#endif
