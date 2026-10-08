idf_component_register(SRCS "TempSensorAndDisplay.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c glyphs display)
