# Design

## Architecture

`battery_esp_bq24074` is an ESP-IDF driver for monitoring batteries connected to a Texas Instruments BQ24074 charge controller.

### Three-Layer Battery Monitoring
This component is the middle layer of a three-layer battery monitoring stack:
1. **`battery_charge_percent`** (dependency) — pure C voltage→percentage conversion with lookup curves.
2. **`battery_esp_bq24074`** (this component) — hardware driver that reads ADC voltage and BQ24074 status GPIOs.
3. **Application service** (consumer's responsibility) — integrates battery state into application UI/logic.

### Hardware Interface
- **ADC pin** (`gpio_bat_lvl`): Battery voltage via resistor divider, read using ESP-IDF ADC oneshot API with curve-fitting calibration.
- **!PGOOD pin** (`gpio_pgood`): Active-low signal indicating external power is present.
- **!PCHG pin** (`gpio_pchg`): Active-low signal indicating active charging.

### Monitoring Architecture
- A FreeRTOS task polls ADC and GPIO state at a configurable interval (`update_period_ms`).
- GPIO edge interrupts on !PGOOD and !PCHG wake the task early for responsive state change detection.
- State transitions fire events via a registered callback (`bq24074_callback_t`).
- State is stored in a `bq24074_state_t` struct and copied atomically via `bq24074_get_state()`.

### Event-Driven Model
Events: `VOLTAGE_UPDATE`, `CHARGE_STARTED`, `CHARGE_STOPPED`, `POWER_CONNECTED`, `POWER_DISCONNECTED`. Callbacks are invoked from the monitoring task context (not ISR-safe — use queues if cross-task communication is needed).

## Design Decisions

### Opaque Handle Pattern
The driver uses an opaque handle (`bq24074_handle_t`) to hide internal state. This allows multiple driver instances and prevents direct access to internal fields.

### Init/Start Separation
`bq24074_init()` allocates the driver and copies config. `bq24074_start()` begins hardware interaction. This allows early configuration with deferred start, and simplifies error recovery.

### ADC Calibration
Uses ESP-IDF ADC curve-fitting calibration when available (`ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED`), falling back to linear raw-to-voltage conversion. Calibration provides ±10mV accuracy vs ±100mV without.

### Dependency on battery_charge_percent
Voltage→percentage conversion is delegated to the `battery_charge_percent` library. This avoids duplicating curve logic and allows custom discharge curves via the config.
