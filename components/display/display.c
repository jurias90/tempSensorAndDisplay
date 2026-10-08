//
// Created by Jesus Urias on 10/7/26.
//

#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "display.h"
#include "esp_log.h"

#define COMMAND_BYTE_HEADER 0x00
#define DATA_TRANSFER_BYTE_HEADER 0x40
#define TRANSMIT_TIMEOUT 100
#define SSD1306_MAX_WIDTH 128
#define SSD1306_MAX_HEIGHT 64
#define MAX_VALID_ADDRESS 0x7F
#define SSD1306_MAX_VALID_SPEED 400000
#define MAX_BUFFER_SIZE  (SSD1306_MAX_WIDTH * SSD1306_MAX_HEIGHT / 8 )
#define VERTICAL_FLIP_CODE 0xC8
#define VERTICAL_STANDARD_CODE 0xC0
#define HORIZONTAL_STANDARD_CODE 0xA0
#define HORIZONTAL_FLIP_CODE 0xA1

static const char* TAG = "display";

static const uint8_t zero_window[MAX_BUFFER_SIZE]  = {0};

struct display {
    i2c_master_dev_handle_t screen_handle;
    uint16_t width, height;
    uint8_t *transfer_buf;
    size_t buffer_size;
};

static esp_err_t display_transmit(display_t* display_p,uint8_t header, const uint8_t* payload,size_t length) {
    if (display_p == NULL || (payload == NULL && length > 0)) return ESP_ERR_INVALID_ARG;

    if (length > display_p->buffer_size - 1) return ESP_ERR_INVALID_SIZE;

    display_p->transfer_buf[0] = header;
    if (payload != NULL) memcpy(&display_p->transfer_buf[1], payload, length);
    return i2c_master_transmit(display_p->screen_handle, display_p->transfer_buf, length + 1, TRANSMIT_TIMEOUT + (int)(display_p->buffer_size));
}

static esp_err_t display_command_transmit(display_t* display_p, const uint8_t* payload, const size_t length) {
    return display_transmit(display_p, COMMAND_BYTE_HEADER, payload, length);
}

static esp_err_t display_data_transmit(display_t* display_p, const uint8_t* payload, const size_t length) {
    return display_transmit(display_p, DATA_TRANSFER_BYTE_HEADER, payload, length);
}

static esp_err_t display_set_window(display_t* display_p, const uint8_t start_col, const uint8_t end_col, const uint8_t start_page, const uint8_t end_page) {
    if (display_p == NULL) return ESP_ERR_INVALID_ARG;
    if (start_col > end_col || start_page > end_page) return ESP_ERR_INVALID_ARG;
    if (end_col > (uint8_t)(display_p->width - 1) || end_page > (uint8_t)(display_p->height / 8 - 1)) return ESP_ERR_INVALID_ARG;

    uint8_t data[] = {0x21, start_col,end_col, 0x22, start_page, end_page};
    return display_command_transmit(display_p, data, sizeof(data));
}



static esp_err_t check_config_params(const display_config_t* config) {
    if (config == NULL || config->i2c_bus == NULL) return ESP_ERR_INVALID_ARG;
    if (config->width == 0 || config->height == 0) return ESP_ERR_INVALID_ARG;
    if (config->width > SSD1306_MAX_WIDTH || config->height > SSD1306_MAX_HEIGHT) return ESP_ERR_INVALID_ARG;
    if (config->height % 8 != 0) return ESP_ERR_INVALID_ARG;
    if (config->address > MAX_VALID_ADDRESS || config->address == 0) return ESP_ERR_INVALID_ARG;
    if (config->speed > SSD1306_MAX_VALID_SPEED || config->speed == 0) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

static void display_release(display_t** display_p) {
    if (display_p == NULL || *display_p == NULL) return;
    if ((*display_p)->screen_handle != NULL) {
        esp_err_t rm_dev_res = i2c_master_bus_rm_device((*display_p)->screen_handle);
        if (rm_dev_res != ESP_OK) {
            ESP_LOGW(TAG,"The device failed with the error %s ",esp_err_to_name(rm_dev_res));
        }
    }
    free((*display_p)->transfer_buf);
    (*display_p)->transfer_buf = NULL;
    free(*display_p);
    *display_p = NULL;
}

esp_err_t display_init(const display_config_t* config, display_t** out_display) {
    if (out_display == NULL || config == NULL ) return ESP_ERR_INVALID_ARG;
    *out_display = NULL;
    esp_err_t err = check_config_params(config);
    if (err != ESP_OK) return err;

    display_t* display_p = (display_t*)calloc(1, sizeof(display_t));
    if (display_p == NULL) return ESP_ERR_NO_MEM;
    display_p->width = config->width;
    display_p->height = config->height;
    display_p->buffer_size = config->width * (config->height/8) + 1;
    display_p->transfer_buf = (uint8_t*)calloc(display_p->buffer_size, sizeof(uint8_t));
    if (display_p->transfer_buf == NULL) {
        free(display_p);
        return ESP_ERR_NO_MEM;
    }
    i2c_device_config_t screen_config = {
        .dev_addr_length =  I2C_ADDR_BIT_LEN_7,
        .device_address = config->address,
        .scl_speed_hz = config->speed
    };

    esp_err_t bus_res = i2c_master_bus_add_device(config->i2c_bus, &screen_config, &display_p->screen_handle);
    if (bus_res != ESP_OK) {
        display_release(&display_p);
        return bus_res;
    }

    uint8_t init[] = {0xAE, 0x20, 0x00,
        (config->horizontal_orientation_flip ? HORIZONTAL_FLIP_CODE : HORIZONTAL_STANDARD_CODE ),
        0x8D, 0x14, 0xA4, 0xA6,
        (config->vertical_orientation_flip ? VERTICAL_FLIP_CODE : VERTICAL_STANDARD_CODE)};

    esp_err_t init_res = display_command_transmit(display_p, init, sizeof(init));
    if (init_res != ESP_OK) {
        display_release(&display_p);
        return init_res;
    }

    esp_err_t clear_res = display_clear(display_p);
    if (clear_res != ESP_OK) {
        display_release(&display_p);
        return clear_res;
    }

    uint8_t display_on[] = {0xAF};
    esp_err_t on_res = display_command_transmit(display_p, display_on, sizeof(display_on));
    if (on_res != ESP_OK) {
        display_release(&display_p);
        return on_res;
    }

    *out_display = display_p;
    return ESP_OK;

}

esp_err_t display_write_region(display_t* display_p, const uint8_t start_col, const uint8_t end_col, const uint8_t start_page, const uint8_t end_page, const uint8_t* payload, const size_t payload_len) {
    if (display_p == NULL || (payload == NULL && payload_len > 0)) return ESP_ERR_INVALID_ARG;
    if (start_col > end_col || start_page > end_page) return ESP_ERR_INVALID_ARG;
    if ((size_t)((end_col-start_col+1)*(end_page-start_page+1)) != payload_len) return ESP_ERR_INVALID_SIZE;

    esp_err_t set_res = display_set_window(display_p, start_col, end_col, start_page, end_page);
    if (set_res != ESP_OK) return set_res;

    return display_data_transmit(display_p,payload, payload_len);

}

esp_err_t display_clear(display_t* display_p) {
    if (display_p == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t clear_res = display_set_window(display_p, 0, (uint8_t)(display_p->width - 1), 0, (uint8_t)(display_p->height / 8  - 1));
    if (clear_res != ESP_OK) {
        return clear_res;
    }

    esp_err_t zero_res = display_data_transmit(display_p, zero_window, display_p->buffer_size-1);
    if (zero_res != ESP_OK) {
        return zero_res;
    }

    return ESP_OK;
}

void display_deinit(display_t** display_p) {
    display_release(display_p);
}