#include <stdio.h>

#include "display.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "glyphs.h"



#define SCREEN_ADDRESS 0x3c


void app_main(void)
{
    i2c_master_bus_config_t bus_config= {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
        .glitch_ignore_cnt = 7
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config,&bus_handle));


    display_config_t display_config = {
        .i2c_bus = bus_handle,
        .address = SCREEN_ADDRESS,
        .width = 128,
        .height = 64,
        .speed = 100000,
        .horizontal_orientation_flip = true,
        .vertical_orientation_flip = true
    };

    display_t* display_p;
    ESP_ERROR_CHECK(display_init(&display_config, &display_p));

    ESP_ERROR_CHECK(display_write_region(display_p,12,33,1,6,glyph_d2,sizeof(glyph_d2)));
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_ERROR_CHECK(display_clear(display_p));
    display_deinit(&display_p);
}
