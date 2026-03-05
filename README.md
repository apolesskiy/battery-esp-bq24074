# battery_esp_bq24074

ESP-IDF driver for the BQ24074 battery charger IC. Reads battery voltage via ADC and monitors charge/power status via GPIOs. Provides event callbacks on state changes.

## Features

- Periodic ADC-based battery voltage monitoring with calibrated readings
- GPIO interrupt-driven charge and power status detection
- Event callback system for voltage updates and state transitions
- Configurable polling period, task stack, and priority
- Custom battery discharge curves via `battery_charge_percent` dependency

## Installation

### ESP-IDF Component Manager

Add to your project's `idf_component.yml`:

```yaml
dependencies:
  apolesskiy/battery-esp-bq24074:
    git: https://github.com/apolesskiy/battery-esp-bq24074.git
```

### Manual

Copy into your project's `components/` folder. Requires the
[battery-charge-percent](https://github.com/apolesskiy/battery-charge-percent)
component as a sibling or registered dependency.

## Usage

```c
#include "bq24074_driver.h"

void battery_event_handler(bq24074_event_t event, void *ctx) {
    switch (event) {
        case BQ24074_EVENT_VOLTAGE_UPDATE:
            // Periodic voltage reading
            break;
        case BQ24074_EVENT_CHARGE_STARTED:
            // USB power connected and charging
            break;
        case BQ24074_EVENT_CHARGE_STOPPED:
            // Charging complete or disconnected
            break;
        case BQ24074_EVENT_POWER_CONNECTED:
            // External power detected
            break;
        case BQ24074_EVENT_POWER_DISCONNECTED:
            // External power removed
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

    bq24074_handle_t handle;
    bq24074_init(&config, &handle);
    bq24074_set_callback(handle, battery_event_handler, NULL);
    bq24074_start(handle);
}
```

### Custom Charge Curve

Pass a custom discharge curve through the battery config:

```c
static const battery_curve_point_t my_curve[] = {
    {2500, 0}, {3000, 20}, {3500, 60}, {4000, 90}, {4200, 100},
};

bq24074_config_t config = BQ24074_CONFIG_DEFAULT();
config.battery_config.chemistry = BATTERY_CHEM_CUSTOM;
config.battery_config.custom_curve = my_curve;
config.battery_config.custom_curve_len = 5;
// ... set GPIOs and voltage divider params ...
```

## Hardware

The BQ24074 provides two active-low status pins:

| Pin | Signal | Meaning when LOW |
|-----|--------|------------------|
| `!PGOOD` | Power Good | External power is present |
| `!PCHG` | Charge Status | Battery is actively charging |

Battery voltage is read through a resistor divider connected to an ADC-capable GPIO.

## License

MIT
