#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "ssd1306.h"
#include "time.h"
#include "si7021.h"
#include "esp_log.h"

#define I2C_DISPLAY_PORT I2C_NUM_0
#define I2C_SENSOR_PORT I2C_NUM_1

//*Pin mapping*//
#define DISPLAY_SDA_PIN GPIO_NUM_8 // SSD1306 SDA
#define DISPLAY_SCL_PIN GPIO_NUM_9 // SSD1306 SCL
#define SENSOR_SDA_PIN GPIO_NUM_4  // SHT21 SDA
#define SENSOR_SCL_PIN GPIO_NUM_5  // SHT21 SCL

#define SENSOR_I2C_ADDRESS 0x48
#define OLED_I2C_ADDRESS 0x3C
#define MAX 20

#define I2C_DISPLAY_FREQ_HZ 400000
#define I2C_SENSOR_FREQ_HZ 100000

static const char *TAG = "Temperature sensor";
static i2c_dev_t sensor_dev;

typedef struct
{
    float temperature;
    float humidity;
} sensor_measurement;

typedef struct
{
    i2c_port_t port;
    uint8_t address;
} device_info;

QueueHandle_t sensor_measurement_queue;

static void i2c_interface_init(i2c_port_t port, int sda_io, int scl_io, uint32_t freq_hz)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = freq_hz,
    };
    i2c_param_config(port, &conf);
    esp_err_t err = i2c_driver_install(port, conf.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_driver was not init for port %d: %d", port, err);
    }
}

// static void i2c_init_bus(i2c_port_t port, int sda_pin, int scl_pin)
//{
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = sda_pin,
//         .scl_io_num = scl_pin,
//     };
//
//     i2c_param_config(port, &conf);
//     esp_err_t err = i2c_driver_install(port, conf.mode, 0, 0, 0);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "i2c_driver somehow is not functional for port %d: %d", port, err);
//     }
// }

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
    device_info *dev = (device_info*)param;
    sensor_measurement sensor_measurement;

    while (1)
    {
        esp_err_t t_err = si7021_measure_temperature(&sensor_dev, &sensor_measurement.temperature);
        esp_err_t h_err = si7021_measure_humidity(&sensor_dev, &sensor_measurement.humidity);

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

void display_task(void *param)
{
    // Remove unused dev variable  
    sensor_measurement sensor_measurement;
    SSD1306_t dev;  // Add SSD1306 device structure

    // Initialize the display
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xFF);
    ssd1306_display_text_x3(&dev, 0, "Hello", 5, false);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    // DO NOT FORGET ABOUT FREE
    while (1)
    {
        xQueueReceive(sensor_measurement_queue, &sensor_measurement, portMAX_DELAY);
        char *data_string = convert_data_tostring(&sensor_measurement);

        // Show on display
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "Sensor Reading:", 16, false);
        
        char temp_buf[16];
        sprintf(temp_buf, "Temp: %.1f C", sensor_measurement.temperature);
        ssd1306_display_text(&dev, 2, temp_buf, strlen(temp_buf), false);
        
        char hum_buf[16];
        sprintf(hum_buf, "Hum:  %.1f %%", sensor_measurement.humidity);
        ssd1306_display_text(&dev, 3, hum_buf, strlen(hum_buf), false);

        // Log to console
        ESP_LOGI(TAG, "%s", data_string);
        free(data_string);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// TODO: add several tasks to display and measure
void app_main(void)
{
    // Initialize I2C subsystem from esp-idf-lib (uses consistent driver version)
    ESP_ERROR_CHECK(i2cdev_init());
    
    // Create the measurement queue
    sensor_measurement_queue = xQueueCreate(10, sizeof(sensor_measurement));

    // Initialize sensor using the esp-idf-lib approach
    memset(&sensor_dev, 0, sizeof(sensor_dev));
    ESP_ERROR_CHECK(si7021_init_desc(&sensor_dev, I2C_SENSOR_PORT, SENSOR_SDA_PIN, SENSOR_SCL_PIN));

    // Instead of calling i2c_interface_init which uses the new driver API,
    // rely on i2cdev_init which already set up the bus

    // Create the tasks
    xTaskCreate(temperature_measurement_task, "SensorMeasurement", 8192, NULL, 1, NULL);
    xTaskCreate(display_task, "Display", 8192, NULL, 1, NULL);
}