#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO          16        // Set to your SCL pin
#define I2C_MASTER_SDA_IO          15        // Set to your SDA pin
#define I2C_MASTER_NUM             I2C_NUM_0 // I2C port
#define I2C_MASTER_FREQ_HZ         100000
#define I2C_MASTER_TIMEOUT_MS      1000

#define DS3231_ADDR                0x68

static const char *TAG = "DS3231";

// Convert BCD to Decimal
uint8_t bcd_to_dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

// I2C Master Init
void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Convert Decimal to BCD
uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// Set time to DS3231
void ds3231_set_time(uint8_t hour, uint8_t minute, uint8_t second,
                     uint8_t day, uint8_t month, uint16_t year) {
    uint8_t data[7];

    // Convert to BCD format
    data[0] = dec_to_bcd(second);
    data[1] = dec_to_bcd(minute);
    data[2] = dec_to_bcd(hour);
    data[3] = 1; // Day of week is not used here, set to 1 (Monday)
    data[4] = dec_to_bcd(day);
    data[5] = dec_to_bcd(month);
    data[6] = dec_to_bcd(year % 100); // Only last two digits of year

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // start from register 0x00
    i2c_master_write(cmd, data, 7, true);   // write 7 bytes to set time
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    ESP_LOGI(TAG, "Time Set: %02d:%02d:%02d  Date: %02d/%02d/%d",
             hour, minute, second, day, month, year);
}

// Read time from DS3231
void ds3231_read_time() {
    uint8_t data[7];

    // Send register address 0x00
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    // Read 7 bytes
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + 6, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    int seconds = bcd_to_dec(data[0]);
    int minutes = bcd_to_dec(data[1]);
    int hours = bcd_to_dec(data[2]);
    int day = bcd_to_dec(data[4]);
    int month = bcd_to_dec(data[5] & 0x1F); // remove century bit
    int year = bcd_to_dec(data[6]) + 2000;

    ESP_LOGI(TAG, "Time: %02d:%02d:%02d  Date: %02d/%02d/%d", hours, minutes, seconds, day, month, year);
}

void app_main(void) {
    i2c_master_init();
    ds3231_set_time(17, 54, 00, 29, 7, 2025); 
    while (1) {
        ds3231_read_time();
        vTaskDelay(pdMS_TO_TICKS(1000)); // every 1 seconds
    }
}   