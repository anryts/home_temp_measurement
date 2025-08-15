# AI agent instructions for this repository

Project type: ESP-IDF (Espressif) firmware project with FreeRTOS tasks, I2C sensors, and OLED display.

Big picture
- The app is a modified ESP-IDF "blink" example evolved into a temperature/humidity monitor.
- Core logic lives in `main/blink_example_main.c` and runs two FreeRTOS tasks: a sensor reader (Si7021/SHT21) and an SSD1306 OLED display updater, communicating via a FreeRTOS queue.
- I2C is used for both peripherals. The sensor uses the `esp-idf-lib/si7021` (i2cdev-based) driver; the display uses `nopnop2002/ssd1306`.
- Dependency management uses ESP-IDF Component Manager (`main/idf_component.yml`) with a committed `dependencies.lock` for reproducible builds.

Architecture and patterns
- Tasks:
  - `temperature_measurement_task(void *param)`: reads sensor via `si7021_measure_*` and sends `sensor_measurement` structs to `sensor_measurement_queue`.
  - `display_task(void *param)`: receives from the queue and renders text on SSD1306 with helper functions (`ssd1306_display_text*`, etc.).
- Globals:
  - `static i2c_dev_t sensor_dev;` and `static SSD1306_t display_dev;` are initialized in `app_main` and their addresses passed to tasks.
  - `QueueHandle_t sensor_measurement_queue` created in `app_main` before starting tasks.
- I2C setup:
  - Keep it simple: initialize i2cdev once (`ESP_ERROR_CHECK(i2cdev_init());`).
  - Initialize device descriptors: `si7021_init_desc(&sensor_dev, I2C_SENSOR_PORT, SENSOR_SDA_PIN, SENSOR_SCL_PIN);` and `ssd1306_init_desc(&display_dev, I2C_DISPLAY_PORT, DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);` then `ssd1306_init(&display_dev, 128, 64);`.
  - Avoid mixing `driver/i2c_master.h` APIs with i2cdev-based drivers unless you fully migrate.
- Logging: use `ESP_LOGI(TAG, ...)`/`ESP_LOGE` and guard malloc results when converting to strings.

Conventions and pitfalls specific to this repo
- Only wrap esp_err_t-returning calls with `ESP_ERROR_CHECK`. FreeRTOS functions like `xTaskCreate`, `xQueueCreate`, and `vTaskDelay` do not return `esp_err_t`.
- Create queues before starting tasks; using a NULL queue handle will assert in `xQueueSend/xQueueReceive`.
- Use static/global device descriptor storage for task parameters. Do not pass addresses of stack locals to tasks.
- Target-specific sdkconfig is managed via `sdkconfig.defaults.*` files. The main `sdkconfig` may be ignored in git; adjust defaults per target as needed.

Build, flash, monitor
- VS Code tasks are preconfigured:
  - Build: "ESP-IDF: Build"
  - Flash: "ESP-IDF: Flash" (depends on Build)
  - Monitor: "ESP-IDF: Monitor"
  - Configure: "ESP-IDF: Configure" (menuconfig)
- CLI examples:
  - `idf.py set-target esp32c3` (or your board)
  - `idf.py reconfigure`
  - `idf.py build flash monitor`

Tests
- `pytest_blink.py` uses `pytest-embedded-idf` harness. It expects the default example binary names; adapt if the app name changes.

External components and integration
- Component Manager pulls:
  - `espressif/led_strip` (not heavily used in current main)
  - `esp-idf-lib/si7021` for the sensor (i2cdev API)
  - `nopnop2002/ssd1306` for OLED (initialized with `ssd1306_init_desc`/`ssd1306_init`)
- Lock file `dependencies.lock` should be committed to pin versions across machines.

Common errors and quick fixes
- Compile error "a value of type 'void' cannot be used to initialize esp_err_t": do not wrap void/FreeRTOS calls with `ESP_ERROR_CHECK`.
- Runtime queue assert: ensure `sensor_measurement_queue = xQueueCreate(...);` runs before any task uses it.
- I2C timeouts or NACK: verify correct pins/macros: `DISPLAY_SDA_PIN/DISPLAY_SCL_PIN`, `SENSOR_SDA_PIN/SENSOR_SCL_PIN`, I2C ports, and 3.3V pull-ups.

File map
- `main/blink_example_main.c`: main app and tasks
- `main/idf_component.yml`: component dependencies
- `sdkconfig.defaults*`: per-target configs
- `.vscode/` and tasks: run/build helpers

Contribution notes for AI agents
- Keep changes minimal and focused; do not switch I2C stacks unless requested.
- If you add a new component, declare it in `main/idf_component.yml` and run `idf.py reconfigure` to fetch it.
- When adding tasks or queues, prefer static/global storage and check return values explicitly.
