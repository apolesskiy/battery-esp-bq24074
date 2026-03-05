/*
 * SPDX-FileCopyrightText: 2026 Aleksey Polesskiy
 * SPDX-License-Identifier: MIT
 */

#include "bq24074_driver.h"

#include <stdlib.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bq24074";

/** Internal driver state (opaque to callers). */
struct bq24074_driver {
  bq24074_config_t config;
  TaskHandle_t task_handle;
  bq24074_callback_t callback;
  void *callback_ctx;
  bq24074_state_t state;
  bool running;
};

/* ── Forward declarations ─────────────────────────────────────── */

static void task_entry(void *arg);
static void task_loop(bq24074_handle_t drv);
static bq24074_state_t read_hardware(bq24074_handle_t drv);
static void gpio_isr_handler(void *arg);

/* ── Public API ────────────────────────────────────────────────── */

esp_err_t bq24074_init(const bq24074_config_t *config,
                       bq24074_handle_t *out_handle) {
  ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG,
                      TAG, "config and out_handle must not be NULL");

  bq24074_handle_t drv = calloc(1, sizeof(struct bq24074_driver));
  ESP_RETURN_ON_FALSE(drv, ESP_ERR_NO_MEM, TAG, "alloc failed");

  memcpy(&drv->config, config, sizeof(bq24074_config_t));
  *out_handle = drv;
  return ESP_OK;
}

void bq24074_deinit(bq24074_handle_t handle) {
  if (!handle) {
    return;
  }
  bq24074_stop(handle);
  free(handle);
}

esp_err_t bq24074_start(bq24074_handle_t handle) {
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "NULL handle");
  if (handle->running) {
    return ESP_ERR_INVALID_STATE;
  }

  /* Configure charge-status GPIOs as inputs with edge interrupts. */
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << handle->config.gpio_pgood) |
                      (1ULL << handle->config.gpio_pchg),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  esp_err_t err = gpio_config(&io_conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(err));
    return err;
  }

  /* Install ISR service (ignore INVALID_STATE — may already be installed). */
  err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "ISR service install failed: %s", esp_err_to_name(err));
    return err;
  }
  gpio_isr_handler_add(handle->config.gpio_pgood, gpio_isr_handler, handle);
  gpio_isr_handler_add(handle->config.gpio_pchg, gpio_isr_handler, handle);

  handle->running = true;

  BaseType_t ret = xTaskCreate(
      task_entry, "battery", handle->config.task_stack_size,
      handle, handle->config.task_priority, &handle->task_handle);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Task creation failed");
    handle->running = false;
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Started (period=%lums)",
           (unsigned long)handle->config.update_period_ms);
  return ESP_OK;
}

void bq24074_stop(bq24074_handle_t handle) {
  if (!handle || !handle->running) {
    return;
  }
  handle->running = false;

  gpio_isr_handler_remove(handle->config.gpio_pgood);
  gpio_isr_handler_remove(handle->config.gpio_pchg);

  if (handle->task_handle) {
    xTaskNotifyGive(handle->task_handle);
    vTaskDelay(pdMS_TO_TICKS(100));
    handle->task_handle = NULL;
  }

  ESP_LOGI(TAG, "Stopped");
}

esp_err_t bq24074_get_state(bq24074_handle_t handle,
                            bq24074_state_t *out_state) {
  ESP_RETURN_ON_FALSE(handle && out_state, ESP_ERR_INVALID_ARG,
                      TAG, "NULL argument");
  *out_state = handle->state;
  return ESP_OK;
}

esp_err_t bq24074_set_callback(bq24074_handle_t handle,
                               bq24074_callback_t callback,
                               void *user_ctx) {
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "NULL handle");
  handle->callback = callback;
  handle->callback_ctx = user_ctx;
  return ESP_OK;
}

esp_err_t bq24074_request_update(bq24074_handle_t handle) {
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "NULL handle");
  if (handle->task_handle) {
    xTaskNotifyGive(handle->task_handle);
  }
  return ESP_OK;
}

/* ── Internal helpers ──────────────────────────────────────────── */

static void IRAM_ATTR gpio_isr_handler(void *arg) {
  bq24074_handle_t drv = (bq24074_handle_t)arg;
  if (drv->task_handle) {
    BaseType_t higher_prio_woken = pdFALSE;
    vTaskNotifyGiveFromISR(drv->task_handle, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
  }
}

static void task_entry(void *arg) {
  task_loop((bq24074_handle_t)arg);
  vTaskDelete(NULL);
}

/**
 * @brief Main monitoring loop — reads ADC and GPIO state periodically.
 *
 * Configures ADC with calibration on entry, then loops reading voltage
 * and GPIO status. Fires event callbacks on state changes. Sleeps until
 * the next polling period or a GPIO interrupt wakes the task early.
 */
static void task_loop(bq24074_handle_t drv) {
  /* Configure ADC for battery voltage reading. */
  adc_oneshot_unit_handle_t adc_handle = NULL;
  adc_oneshot_unit_init_cfg_t adc_cfg = {
      .unit_id = ADC_UNIT_2,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  esp_err_t err = adc_oneshot_new_unit(&adc_cfg, &adc_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(err));
    return;
  }

  /* Determine ADC channel from GPIO number. */
  adc_unit_t unit;
  adc_channel_t channel;
  err = adc_oneshot_io_to_channel(drv->config.gpio_bat_lvl, &unit, &channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GPIO-to-channel failed: %s", esp_err_to_name(err));
    adc_oneshot_del_unit(adc_handle);
    return;
  }

  adc_oneshot_chan_cfg_t chan_cfg = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  adc_oneshot_config_channel(adc_handle, channel, &chan_cfg);

  /* Set up ADC calibration for accurate millivolt readings. */
  adc_cali_handle_t cali_handle = NULL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cali_cfg = {
      .unit_id = ADC_UNIT_2,
      .chan = channel,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
#endif

  while (drv->running) {
    bq24074_state_t prev = drv->state;
    drv->state = read_hardware(drv);

    /* Read ADC raw value and convert to calibrated millivolts. */
    int raw = 0;
    adc_oneshot_read(adc_handle, channel, &raw);
    int voltage_mv = 0;
    if (cali_handle) {
      adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
    } else {
      voltage_mv = (raw * drv->config.battery_config.adc_max_mv) / 4095;
    }

    drv->state.voltage_mv = battery_mv_from_adc_mv(
        &drv->config.battery_config, (uint16_t)voltage_mv);
    drv->state.percent = battery_charge_percent_from_adc_mv(
        &drv->config.battery_config, (uint16_t)voltage_mv);

    /* Fire event callbacks on state transitions. */
    if (drv->callback) {
      if (prev.charging != drv->state.charging) {
        drv->callback(drv->state.charging
                          ? BQ24074_EVENT_CHARGE_STARTED
                          : BQ24074_EVENT_CHARGE_STOPPED,
                      drv->callback_ctx);
      }
      if (prev.power_connected != drv->state.power_connected) {
        drv->callback(drv->state.power_connected
                          ? BQ24074_EVENT_POWER_CONNECTED
                          : BQ24074_EVENT_POWER_DISCONNECTED,
                      drv->callback_ctx);
      }
      drv->callback(BQ24074_EVENT_VOLTAGE_UPDATE, drv->callback_ctx);
    }

    /* Wait for next update period or GPIO interrupt. */
    ulTaskNotifyTake(pdTRUE,
                     pdMS_TO_TICKS(drv->config.update_period_ms));
  }

  if (cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_delete_scheme_curve_fitting(cali_handle);
#endif
  }
  adc_oneshot_del_unit(adc_handle);
}

/**
 * @brief Read BQ24074 GPIO status pins.
 *
 * PGOOD and PCHG are active-low signals from the BQ24074.
 */
static bq24074_state_t read_hardware(bq24074_handle_t drv) {
  bq24074_state_t s = drv->state;
  s.power_connected = gpio_get_level(drv->config.gpio_pgood) == 0;
  s.charging = gpio_get_level(drv->config.gpio_pchg) == 0;
  return s;
}
