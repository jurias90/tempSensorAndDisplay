//
// Created by Jesus Urias on 10/7/26.
//

#ifndef TEMPSENSORANDDISPLAY_DISPLAY_H
#define TEMPSENSORANDDISPLAY_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct display display_t;

typedef struct display_config {
    uint8_t address;
    uint16_t width, height;
    uint32_t speed;
    bool vertical_orientation_flip, horizontal_orientation_flip;
    i2c_master_bus_handle_t i2c_bus;
} display_config_t ;

esp_err_t display_init(const display_config_t* config, display_t** out_display);

esp_err_t display_write_region(display_t* display_p, const uint8_t start_col, const uint8_t end_col,
    const uint8_t start_page, const uint8_t end_page,const uint8_t* payload, const size_t payload_len);

esp_err_t display_clear(display_t* display_p);

void display_deinit(display_t** display_p);
#endif //TEMPSENSORANDDISPLAY_DISPLAY_H
