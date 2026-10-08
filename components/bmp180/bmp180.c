//
// Created by Jesus Urias on 10/8/26.
//

#include <stdlib.h>

#include "bmp180.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BMP_180_CALIBRATION_START 0xAA
#define BMP_180_CONTROL_REGISTER 0xF4
#define BMP_180_TEMPERATURE_COMMAND 0x2E
#define BMP_180_RESULT_REGISTER 0xF6
#define BMP_180_CHIP_ID_REGISTER 0xD0
#define BMP_180_CHIP_ID_VALUE 0x55
#define BMP_180_TIMEOUT_MS 100
#define BMP_180_MAX_SPEED_HZ 400000
#define BMP_180_ADDRESS 0x77
#define BMP_180_CALIBRATION_LENGTH 22

struct bmp180 {
    i2c_master_dev_handle_t dev_handle;
    uint16_t ac4, ac5,ac6;
    int16_t ac1,ac2,ac3, b1,b2, mb,mc,md;
};

static const char* TAG = "bmp180";

static int32_t bmp180_compute_tenths(bmp180_t *s, uint16_t ut) {
    int32_t x1 = (ut - s->ac6) * s->ac5 / 32768;
    if (x1+ s->md  ==0) {
        return INT32_MIN;
    }
    int32_t x2 = s->mc * 2048 / (x1 + s->md);
    int32_t b5 = x1 + x2;
    return (b5 + 8)/ 16;
}

static esp_err_t bmp180_read_regs(bmp180_t *sensor, const uint8_t register_address, uint8_t* register_buffer, size_t register_len) {
    if (sensor == NULL || register_buffer == NULL || register_len ==  0 ) { return ESP_ERR_INVALID_ARG;}
    esp_err_t check_req = i2c_master_transmit_receive(sensor->dev_handle, &register_address, 1, register_buffer, register_len, BMP_180_TIMEOUT_MS);
    return check_req;
}

static esp_err_t bmp180_write_reg(bmp180_t* sensor, uint8_t register_address,uint8_t sensor_req) {
    if (sensor == NULL) { return ESP_ERR_INVALID_ARG; }
    uint8_t req[] = {register_address, sensor_req};
    esp_err_t transmit_res = i2c_master_transmit (sensor->dev_handle, req, sizeof(req), BMP_180_TIMEOUT_MS);
    return transmit_res;
}

static void bmp180_sensor_release(bmp180_t** sensor) {
    if (sensor == NULL || *sensor == NULL) { return; }
    if ((*sensor)->dev_handle != NULL) {
       esp_err_t dev_rm_res = i2c_master_bus_rm_device((*sensor)->dev_handle);
       if (dev_rm_res != ESP_OK) {
           ESP_LOGW(TAG, "i2c_master_bus_rm_device failed with error %s", esp_err_to_name(dev_rm_res));
       }
    }
    free(*sensor);
    *sensor = NULL;
}

static uint16_t bmp180_combine_bytes(uint8_t a, uint8_t b) {
    uint16_t res = (uint16_t)a;
    res <<= 8;
    res |= b;
    return res;
}



void bmp180_deinit(bmp180_t** sensor) {
    bmp180_sensor_release(sensor);
}


esp_err_t bmp180_init(const bmp180_config_t* config_p ,bmp180_t** out_sensor) {
    if (out_sensor == NULL || config_p == NULL) { return ESP_ERR_INVALID_ARG; }
    *out_sensor = NULL;

    if (config_p->i2c_bus == NULL) { return ESP_ERR_INVALID_ARG; }
    if (config_p->speed > BMP_180_MAX_SPEED_HZ || config_p->speed == 0) { return ESP_ERR_INVALID_ARG;}
    if (config_p->i2c_address != BMP_180_ADDRESS) { return ESP_ERR_INVALID_ARG; }

    bmp180_t* s= (bmp180_t*) calloc(1,sizeof(bmp180_t));
    if (s == NULL) { return ESP_ERR_NO_MEM; }

    i2c_device_config_t sensor_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config_p->i2c_address,
        .scl_speed_hz =  config_p->speed
    };

    esp_err_t add_device_res = i2c_master_bus_add_device(config_p->i2c_bus, &sensor_config,&s->dev_handle);
    if (add_device_res != ESP_OK) {
        bmp180_sensor_release(&s);
        return add_device_res;
    }
    uint8_t testRead = 0;
    esp_err_t check_sensor_res = bmp180_read_regs(s, BMP_180_CHIP_ID_REGISTER, &testRead, 1 );
    if (check_sensor_res != ESP_OK) {
        bmp180_sensor_release(&s);
        return check_sensor_res;
    }

    if (testRead != BMP_180_CHIP_ID_VALUE) {
        ESP_LOGW(TAG, "BMP180 sensor ID does not match chip ID. Value Received: %02X", testRead);
        bmp180_sensor_release(&s);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t cal_val[BMP_180_CALIBRATION_LENGTH] = {0};
    esp_err_t check_cal_res = bmp180_read_regs(s, BMP_180_CALIBRATION_START, cal_val, BMP_180_CALIBRATION_LENGTH );
    if (check_cal_res != ESP_OK) {
        bmp180_sensor_release(&s);
        return check_cal_res;
    }


    for (int i =0; i < BMP_180_CALIBRATION_LENGTH; i+=2) {
        uint16_t res = bmp180_combine_bytes(cal_val[i],cal_val[i+1]);
        if (res == 0xFFFF || res == 0x0000) {
            bmp180_sensor_release(&s);
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    s->ac1 = (int16_t)bmp180_combine_bytes(cal_val[0], cal_val[1]);
    s->ac2 = (int16_t)bmp180_combine_bytes(cal_val[2], cal_val[3]);
    s->ac3 = (int16_t)bmp180_combine_bytes(cal_val[4], cal_val[5]);
    s->ac4 = bmp180_combine_bytes(cal_val[6], cal_val[7]);
    s->ac5 = bmp180_combine_bytes(cal_val[8], cal_val[9]);
    s->ac6 = bmp180_combine_bytes(cal_val[10], cal_val[11]);
    s->b1 = (int16_t)bmp180_combine_bytes(cal_val[12], cal_val[13]);
    s->b2 = (int16_t)bmp180_combine_bytes(cal_val[14], cal_val[15]);
    s->mb = (int16_t)bmp180_combine_bytes(cal_val[16], cal_val[17]);
    s->mc = (int16_t)bmp180_combine_bytes(cal_val[18], cal_val[19]);
    s->md = (int16_t)bmp180_combine_bytes(cal_val[20], cal_val[21]);

    ESP_LOGD(TAG, "ac1 = %d, ac2 = %d, ac3 = %d, ac4 = %d, ac5 = %d, ac6 = %d, b1 = %d, b2 = %d, mb = %d, mc = %d, md = %d", s->ac1, s->ac2, s->ac3, s->ac4, s->ac5, s->ac6,s->b1, s->b2, s->mb, s->mc, s->md);
    *out_sensor = s;

    return ESP_OK;
}

esp_err_t bmp180_read_temperature(bmp180_t *sensor, int32_t* temperature_in_tenths) {
    if (sensor == NULL || temperature_in_tenths == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t write_req = bmp180_write_reg(sensor, BMP_180_CONTROL_REGISTER, BMP_180_TEMPERATURE_COMMAND);
    if (write_req != ESP_OK) {
       return write_req;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t res[2] = {0};
    esp_err_t write_res = bmp180_read_regs(sensor, BMP_180_RESULT_REGISTER, res, 2);

    if (write_res != ESP_OK) {
        return write_res;
    }
    uint16_t ut = bmp180_combine_bytes(res[0], res[1]);
    int32_t temperature_res = bmp180_compute_tenths(sensor, ut);

    if (temperature_res  == INT32_MIN) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *temperature_in_tenths = temperature_res;

    ESP_LOGD(TAG, "temperature_in_tenths = %d", *temperature_in_tenths);
    return ESP_OK;
}