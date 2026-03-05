## Technology Stack
* ESP-IDF component (C, requires ADC/GPIO hardware).
* Compatible with esp-idf v5.5+.
* Target: ESP32 family (any variant with ADC2 and GPIO).
* Depends on `battery_charge_percent` component for voltage→percentage conversion.

## Agent-First Development
* Documentation should be readable by humans, but optimized for consumption by AI agents.
* Avoid including code in design documents.

## Style
* Use spaces for indentation. Indentation width is 2 spaces.
* C source uses `.c` extension, C headers use `.h`.
* Naming: `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for constants/macros, `snake_case_t` for typedefs.
* Use `#pragma once` for header guards.
* Use SPDX license headers.

## Code Structure
* Prefer small, iterative code changes.
* Keep function length under 50 lines where possible.
* Each public function must have a Doxygen documentation comment.
* Use ESP-IDF error handling patterns (`ESP_RETURN_ON_ERROR`, `ESP_RETURN_ON_FALSE`).
* Use `esp_log.h` logging with static TAG.

## Testing
* This is a hardware driver — most functionality requires on-device testing.
* Host-based tests in `host_test/` — limited to testing config defaults and data structures.
* Run host tests: `cd host_test && cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure`

## Definition of Done
* The component builds as part of an ESP-IDF project.
* Host tests pass.
* On-device tests documented; hardware-dependent tests noted.

## Progress Checkpoints
* Each commit description should start with "[Agent]".
