//
// Created by Jesus Urias on 10/8/26.
//

#ifndef TEMPSENSORANDDISPLAY_BMP180_H
#define TEMPSENSORANDDISPLAY_BMP180_H

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct bmp180 bmp180_t;

typedef struct bmp180_config {
    i2c_master_bus_handle_t i2c_bus;
    uint32_t speed;
    uint8_t i2c_address;
} bmp180_config_t;

esp_err_t bmp180_init(const bmp180_config_t *config, bmp180_t** out_sensor);

void bmp180_deinit(bmp180_t** sensor_p);

esp_err_t bmp180_read_temperature(bmp180_t *sensor, int32_t* temperature_in_tenths);

#endif //TEMPSENSORANDDISPLAY_BMP180_H
