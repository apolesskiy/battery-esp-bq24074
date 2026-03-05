#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
  BATTERY_CHEM_LIION = 0,
  BATTERY_CHEM_CUSTOM,
} battery_chemistry_t;

typedef struct {
  uint16_t voltage_mv;
  uint8_t percent;
} battery_curve_point_t;

typedef struct {
  battery_chemistry_t chemistry;
  uint16_t adc_max_mv;
  uint16_t divider_r1_kohm;
  uint16_t divider_r2_kohm;
  uint16_t voltage_min_mv;
  uint16_t voltage_max_mv;
  const battery_curve_point_t* custom_curve;
  size_t custom_curve_len;
} battery_charge_percent_config_t;

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t battery_charge_percent_from_adc_mv(
    const battery_charge_percent_config_t* config, uint16_t adc_mv) {
  (void)config; (void)adc_mv; return 50;
}

static inline uint16_t battery_mv_from_adc_mv(
    const battery_charge_percent_config_t* config, uint16_t adc_mv) {
  (void)config; (void)adc_mv; return 3700;
}

#ifdef __cplusplus
}
#endif
