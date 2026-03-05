idf_component_register(
  SRCS "src/bq24074_driver.c"
  INCLUDE_DIRS "include"
  REQUIRES log driver esp_adc esp_driver_gpio freertos battery-charge-percent
)
