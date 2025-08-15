#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "time.h"
#include "si7021.h"
#include "esp_log.h"
#include "esp_lcd_panel_ssd1306.h"
#include "driver/i2c_master.h"
#include <esp_lcd_io_i2c.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/i2c.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
// #include "../lv_examples.h"

#define I2C_DISPLAY_PORT I2C_NUM_0
#define I2C_SENSOR_PORT I2C_NUM_1

//*Pin mapping*//
#define DISPLAY_SDA_PIN GPIO_NUM_8 // SSD1306 SDA
#define DISPLAY_SCL_PIN GPIO_NUM_9 // SSD1306 SCL
#define SENSOR_SDA_PIN GPIO_NUM_4  // SHT21 SDA
#define SENSOR_SCL_PIN GPIO_NUM_5  // SHT21 SCL
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

#define SENSOR_I2C_ADDRESS 0x48
#define OLED_I2C_ADDRESS 0x3C
#define MAX 20

#define I2C_DISPLAY_FREQ_HZ 400000
#define I2C_SENSOR_FREQ_HZ 100000

static const char *TAG = "Temperature sensor";
static i2c_dev_t sensor_dev;

extern void example_lvgl_demo_ui(lv_disp_t *disp);

typedef struct
{
    float temperature;
    float humidity;
} sensor_measurement;

QueueHandle_t sensor_measurement_queue;

char *convert_data_tostring(sensor_measurement *data)
{
    int needed = snprintf(NULL, 0, "Temp: %.2f C, Hm: %.2f", data->temperature, data->humidity);

    // Allocate buffer, +1 for '\0'
    char *buffer = malloc(needed + 1);
    if (buffer == NULL)
    {
        return NULL; // malloc failed, we are doomed
    }

    snprintf(buffer, needed + 1, "Temp: %.2f C, Hm: %.2f", data->temperature, data->humidity);
    return buffer;
}

void temperature_measurement_task(void *param)
{
    i2c_dev_t *dev = (i2c_dev_t *)param;
    sensor_measurement sensor_measurement;

    while (1)
    {
        esp_err_t t_err = si7021_measure_temperature(dev, &sensor_measurement.temperature);
        esp_err_t h_err = si7021_measure_humidity(dev, &sensor_measurement.humidity);

        if (t_err == ESP_OK && h_err == ESP_OK)
        {
            // put into queue
            xQueueSend(sensor_measurement_queue, &sensor_measurement, portMAX_DELAY);
        }
        else
        {
            // printf("Sensor error! Code: T=%d, H=%d\n", t_err, h_err);
            ESP_LOGI(TAG, "Something bad happened");
        }

        // Always delay inside the loop to avoid watchdog timeout
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// void display_task(void *param)
// {
//     sensor_measurement sensor_measurement;
//     esp_lcd_panel_io_i2c_config_t io_config =

//         // Initialize the display
//         //vTaskDelay(3000 / portTICK_PERIOD_MS);

//     while (1)
//     {
//         xQueueReceive(sensor_measurement_queue, &sensor_measurement, portMAX_DELAY);
//         char *data_string = convert_data_tostring(&sensor_measurement);

//         // Log to console (guard against NULL)
//         if (data_string)
//         {
//             ESP_LOGI(TAG, "%s", data_string);
//             free(data_string);
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Failed to alloc log string");
//         }

//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }

// TODO: add several tasks to display and measure
// TODO: go to new version of api for i2c
// void app_main(void)
// {
//     ESP_ERROR_CHECK(i2cdev_init()); // init i2cdev once

//     // Init I2C descriptors for sensor and display (old/simple API)
//     memset(&sensor_dev, 0, sizeof(sensor_dev));

//     //can be used, due to drivers missmatch
//     //i2c_master_init(&display_dev, DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, CONFIG_RESET_GPIO);
//     ESP_ERROR_CHECK(si7021_init_desc(&sensor_dev, I2C_SENSOR_PORT, SENSOR_SDA_PIN, SENSOR_SCL_PIN));

//     //ESP_ERROR_CHECK(ssd1306_init(&display_dev, 128, 64));
//     //memset(&display_dev, 0, sizeof(display_dev));
//     // Create the queue BEFORE starting tasks

//     sensor_measurement_queue = xQueueCreate(5, sizeof(sensor_measurement));
//     configASSERT(sensor_measurement_queue);

//     // Tasks (pass pointers to static globals)
//     //xTaskCreate(display_task, "Display", 8192, &display_dev, 1, NULL);
//     xTaskCreate(temperature_measurement_task, "SensorMeasurement", 8192, &sensor_dev, 1, NULL);
// }

void app_main(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;

    /*I2C Bus initialization */
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT, // Clock source for I2C peripheral
        .glitch_ignore_cnt = 7,            // Help with noise lines?
        .i2c_port = I2C_DISPLAY_PORT,
        .sda_io_num = DISPLAY_SDA_PIN,
        .scl_io_num = DISPLAY_SCL_PIN,
        .flags.enable_internal_pullup = true, // enable internal pull-up resistors on SDA/SCL
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
    /*END of I2C Bus initialization */

    /*Panel IO (I2C to Display)
    how to talk to our SSD1306 over I2C
    */
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDRESS,
        .scl_speed_hz = I2C_DISPLAY_FREQ_HZ,
        .control_phase_bytes = 1, // According to SSD1306 datasheet
        .lcd_cmd_bits = 8,        // According to SSD1306 datasheet
        .lcd_param_bits = 8,      // According to SSD1306 datasheet
        .dc_bit_offset = 6,       //
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));
    /*Panel IO (I2C to Display)*/

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1, // No reset GPIO
    };

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = 64,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = 128 * 64,
        .double_buffer = true,
        .hres = 128,
        .vres = 64,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false},
        //.flags = {
        //    .swap_bytes = false ,     /*!< Allocated LVGL buffer will be DMA capable */
        //    .sw_rotate = false, /*!< Allocated LVGL buffer will be in PSRAM */
        //}
    };

    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    ESP_LOGI(TAG, "Display LVGL Scroll Text");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(0))
    {
        /* Rotation of the screen */
        lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_0);

        example_lvgl_demo_ui(disp);
        // Release the mutex
        lvgl_port_unlock();
    }
}
