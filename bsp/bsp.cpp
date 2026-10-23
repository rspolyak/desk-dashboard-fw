#include "bsp/bsp.hpp"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_attr.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sht4x.h"
#include <algorithm>
#include <cstddef>

/**
 * @file
 * @brief Board support for the desk dashboard PCB with ESP32-S3 (github.com/rspolyak/desk-dashboard-hw).
 */
namespace bsp {
namespace {
constexpr const char* log_tag = "bsp";
constexpr ledc_mode_t backlight_speed_mode = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t backlight_channel = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t backlight_duty_resolution = LEDC_TIMER_10_BIT;
constexpr std::uint32_t backlight_duty_max = (1U << backlight_duty_resolution) - 1;
constexpr spi_host_device_t display_spi_host = SPI2_HOST;
constexpr int display_width = 480;
constexpr int display_height = 320;
constexpr adc_unit_t light_adc_unit = ADC_UNIT_1;
constexpr adc_channel_t light_adc_channel = ADC_CHANNEL_8;

/**
 * @brief The ADC unit and its calibration used to read the light sensor.
 */
struct LightSensor {
    adc_oneshot_unit_handle_t unit = nullptr;
    adc_cali_handle_t calibration = nullptr;
};

// Static: hardware singleton, created by display_init() and used by display_draw().
esp_lcd_panel_handle_t display_panel = nullptr;
// Static: hardware singleton, created by climate_init() and used by climate_read().
sht4x_handle_t climate_sensor = nullptr;
// Static: hardware singleton, created by light_init() and used by light_read().
LightSensor light_sensor = {};

/**
 * @brief Starts the SPI bus the display is on, sized for one full frame.
 * @return ESP_OK, or the error from the SPI driver.
 */
esp_err_t start_display_bus()
{
    constexpr gpio_num_t sck_pin = GPIO_NUM_48;
    constexpr gpio_num_t mosi_pin = GPIO_NUM_41;
    constexpr int frame_bytes = display_width * display_height * sizeof(std::uint16_t);

    spi_bus_config_t bus_config = {};
    bus_config.sclk_io_num = sck_pin;
    bus_config.mosi_io_num = mosi_pin;
    bus_config.miso_io_num = GPIO_NUM_NC;
    bus_config.quadwp_io_num = GPIO_NUM_NC;
    bus_config.quadhd_io_num = GPIO_NUM_NC;
    bus_config.max_transfer_sz = frame_bytes;

    const esp_err_t status = spi_bus_initialize(display_spi_host, &bus_config, SPI_DMA_CH_AUTO);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display SPI bus setup failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Wakes the task waiting in display_draw(); called from the SPI interrupt when its pixels have been sent.
 * @param[in] io Unused.
 * @param[in] event Unused.
 * @param[in] context The drawing task, as given to create_display_link().
 * @return true if the woken task has a higher priority, so the interrupt switches to it on exit.
 */
bool IRAM_ATTR on_pixels_sent([[maybe_unused]] esp_lcd_panel_io_handle_t io,
                              [[maybe_unused]] esp_lcd_panel_io_event_data_t* event, void* context)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(context), &higher_priority_task_woken);
    return higher_priority_task_woken == pdTRUE;
}

/**
 * @brief Creates the SPI link to the display controller on its chip-select and data/command pins.
 * @param[in] drawing_task Task that calls display_draw(); woken when each transfer has finished.
 * @param[out] io The created link.
 * @return ESP_OK, or the error from the LCD driver.
 */
esp_err_t create_display_link(TaskHandle_t drawing_task, esp_lcd_panel_io_handle_t& io)
{
    constexpr gpio_num_t cs_pin = GPIO_NUM_40;
    constexpr gpio_num_t dc_pin = GPIO_NUM_39;
    constexpr int spi_mode = 0;
    constexpr unsigned int spi_clock_hz = 20 * 1000 * 1000;
    constexpr std::size_t transaction_queue_depth = 10;
    constexpr int command_bits = 8;
    constexpr int parameter_bits = 8;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = cs_pin;
    io_config.dc_gpio_num = dc_pin;
    io_config.spi_mode = spi_mode;
    io_config.pclk_hz = spi_clock_hz;
    io_config.trans_queue_depth = transaction_queue_depth;
    io_config.on_color_trans_done = on_pixels_sent;
    io_config.user_ctx = drawing_task;
    io_config.lcd_cmd_bits = command_bits;
    io_config.lcd_param_bits = parameter_bits;

    const esp_err_t status =
        esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(display_spi_host), &io_config, &io);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display SPI link setup failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Creates the ST7796 driver for the panel behind the given link.
 * @param[in] io SPI link to the display controller.
 * @param[out] panel The created panel.
 * @return ESP_OK, or the error from the ST7796 driver.
 */
esp_err_t create_display_panel(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t& panel)
{
    constexpr gpio_num_t reset_pin = GPIO_NUM_38;
    constexpr std::uint32_t bits_per_pixel = 16;

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = reset_pin;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = bits_per_pixel;

    const esp_err_t status = esp_lcd_new_panel_st7796(io, &panel_config, &panel);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "ST7796 driver setup failed: %s", esp_err_to_name(status));
    }
    return status;
}

/**
 * @brief Resets the panel, sends its setup, sets landscape with colour inversion and switches it on.
 * @param[in] panel The panel to set up.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t configure_display_panel(esp_lcd_panel_handle_t panel)
{
    esp_err_t status = esp_lcd_panel_reset(panel);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display reset failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_lcd_panel_init(panel);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display init sequence failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_lcd_panel_invert_color(panel, true);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display colour inversion failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_lcd_panel_swap_xy(panel, true);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display landscape setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = esp_lcd_panel_disp_on_off(panel, true);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "switching the display on failed: %s", esp_err_to_name(status));
        return status;
    }
    return ESP_OK;
}
} // namespace

/**
 * @brief Brings up the board's devices; the backlight starts off, driven by PWM on GPIO47.
 * @return ESP_OK, or the error of the backlight setup step that failed.
 */
esp_err_t init()
{
    constexpr gpio_num_t backlight_pin = GPIO_NUM_47;
    constexpr ledc_timer_t backlight_timer = LEDC_TIMER_0;
    constexpr std::uint32_t backlight_pwm_hz = 5000;
    constexpr std::uint32_t backlight_off_duty = 0;
    constexpr int default_interrupt_flags = 0;

    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = backlight_speed_mode;
    timer_config.duty_resolution = backlight_duty_resolution;
    timer_config.timer_num = backlight_timer;
    timer_config.freq_hz = backlight_pwm_hz;
    timer_config.clk_cfg = LEDC_AUTO_CLK;
    esp_err_t status = ledc_timer_config(&timer_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight PWM timer setup failed: %s", esp_err_to_name(status));
        return status;
    }

    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = backlight_pin;
    channel_config.speed_mode = backlight_speed_mode;
    channel_config.channel = backlight_channel;
    channel_config.timer_sel = backlight_timer;
    channel_config.duty = backlight_off_duty;
    status = ledc_channel_config(&channel_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight PWM channel setup failed: %s", esp_err_to_name(status));
        return status;
    }

    status = ledc_fade_func_install(default_interrupt_flags);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight fade setup failed: %s", esp_err_to_name(status));
        return status;
    }
    return ESP_OK;
}

/**
 * @brief Sets up the ST7796 display in landscape and switches it on; each step logs its own failure.
 * @note The calling task is the one display_draw() wakes after each transfer.
 * @param[out] width Display width in pixels.
 * @param[out] height Display height in pixels.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t display_init(int& width, int& height)
{
    esp_err_t status = start_display_bus();
    if (status != ESP_OK) {
        return status;
    }

    esp_lcd_panel_io_handle_t io = nullptr;
    status = create_display_link(xTaskGetCurrentTaskHandle(), io);
    if (status != ESP_OK) {
        return status;
    }

    status = create_display_panel(io, display_panel);
    if (status != ESP_OK) {
        return status;
    }

    status = configure_display_panel(display_panel);
    if (status != ESP_OK) {
        return status;
    }

    width = display_width;
    height = display_height;
    return ESP_OK;
}

/**
 * @brief Sends a rectangle of pixels to the panel and waits until the SPI transfer has finished.
 * @param[in] x Left edge of the rectangle.
 * @param[in] y Top edge of the rectangle.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] pixels width * height RGB565 values, high byte first, row-major.
 */
void display_draw(int x, int y, int width, int height, const std::uint16_t* pixels)
{
    constexpr std::uint32_t transfer_timeout_ms = 100;

    const esp_err_t status = esp_lcd_panel_draw_bitmap(display_panel, x, y, x + width, y + height, pixels);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "sending pixels to the display failed: %s", esp_err_to_name(status));
        return;
    }

    const std::uint32_t transfers_done = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(transfer_timeout_ms));
    if (transfers_done == 0) {
        ESP_LOGE(log_tag, "display pixel transfer did not finish within %u ms",
                 static_cast<unsigned>(transfer_timeout_ms));
    }
}

/**
 * @brief Fades the display backlight to a new brightness over about one second.
 * @param[in] percent Brightness from 0 (off) to 100 (full); values outside are limited to that range.
 */
void display_backlight(int percent)
{
    constexpr int percent_min = 0;
    constexpr int percent_max = 100;
    constexpr int fade_time_ms = 1000;

    const int limited_percent = std::clamp(percent, percent_min, percent_max);
    const std::uint32_t duty = backlight_duty_max * static_cast<std::uint32_t>(limited_percent) / percent_max;
    esp_err_t status = ledc_set_fade_with_time(backlight_speed_mode, backlight_channel, duty, fade_time_ms);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight fade setup failed: %s", esp_err_to_name(status));
        return;
    }

    status = ledc_fade_start(backlight_speed_mode, backlight_channel, LEDC_FADE_NO_WAIT);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "backlight fade start failed: %s", esp_err_to_name(status));
    }
}

/**
 * @brief Starts the sensor's own I2C bus and sets up the SHT40 temperature and humidity sensor.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t climate_init()
{
    constexpr i2c_port_num_t i2c_port = I2C_NUM_0;
    constexpr gpio_num_t sda_pin = GPIO_NUM_18;
    constexpr gpio_num_t scl_pin = GPIO_NUM_17;
    constexpr std::uint8_t glitch_filter_cycles = 7;
    constexpr std::uint16_t sensor_address = 0x44;
    constexpr std::uint32_t i2c_clock_hz = 100 * 1000;

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = i2c_port;
    bus_config.sda_io_num = sda_pin;
    bus_config.scl_io_num = scl_pin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = glitch_filter_cycles;
    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t status = i2c_new_master_bus(&bus_config, &bus);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "sensor I2C bus setup failed: %s", esp_err_to_name(status));
        return status;
    }

    sht4x_config_t sensor_config = {};
    sensor_config.i2c_address = sensor_address;
    sensor_config.i2c_clock_speed = i2c_clock_hz;
    sensor_config.repeat_mode = SHT4X_REPEAT_HIGH;
    sensor_config.heater_mode = SHT4X_HEATER_OFF;
    status = sht4x_init(bus, &sensor_config, &climate_sensor);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "SHT40 setup failed: %s", esp_err_to_name(status));
        return status;
    }
    return ESP_OK;
}

/**
 * @brief Measures the indoor temperature and relative humidity with the SHT40.
 * @param[out] temperature_c Temperature in degrees Celsius.
 * @param[out] humidity_percent Relative humidity in percent, limited to 0..100 as the datasheet requires.
 * @return ESP_OK, or the error of the measurement.
 */
esp_err_t climate_read(float& temperature_c, float& humidity_percent)
{
    constexpr float humidity_min_percent = 0.0f;
    constexpr float humidity_max_percent = 100.0f;

    float humidity = 0.0f;
    const esp_err_t status = sht4x_get_measurement(climate_sensor, &temperature_c, &humidity);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "SHT40 measurement failed: %s", esp_err_to_name(status));
        return status;
    }

    humidity_percent = std::clamp(humidity, humidity_min_percent, humidity_max_percent);
    return ESP_OK;
}

/**
 * @brief Sets up the ADC that reads the ambient light sensor on GPIO9 (0..1.75 V range), with its factory calibration.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t light_init()
{
    constexpr adc_atten_t attenuation = ADC_ATTEN_DB_6;

    adc_oneshot_unit_init_cfg_t unit_config = {};
    unit_config.unit_id = light_adc_unit;
    esp_err_t status = adc_oneshot_new_unit(&unit_config, &light_sensor.unit);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "light sensor ADC setup failed: %s", esp_err_to_name(status));
        return status;
    }

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = attenuation;
    channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    status = adc_oneshot_config_channel(light_sensor.unit, light_adc_channel, &channel_config);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "light sensor ADC channel setup failed: %s", esp_err_to_name(status));
        return status;
    }

    adc_cali_curve_fitting_config_t calibration_config = {};
    calibration_config.unit_id = light_adc_unit;
    calibration_config.chan = light_adc_channel;
    calibration_config.atten = attenuation;
    calibration_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    status = adc_cali_create_scheme_curve_fitting(&calibration_config, &light_sensor.calibration);
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "light sensor ADC calibration setup failed: %s", esp_err_to_name(status));
        return status;
    }
    return ESP_OK;
}

/**
 * @brief Measures the ambient light from the averaged sensor voltage.
 * @param[out] lux Approximate illuminance; the ALS-PT19 gives about 0.2 uA per lux into 10 kOhm, so 2 mV per lux.
 * @return ESP_OK, or the error of the ADC read that failed.
 */
esp_err_t light_read(float& lux)
{
    constexpr int sample_count = 8;
    constexpr float millivolts_per_lux = 2.0f;

    int sum_mv = 0;
    for (int i = 0; i < sample_count; ++i) {
        int sample_mv = 0;
        const esp_err_t status =
            adc_oneshot_get_calibrated_result(light_sensor.unit, light_sensor.calibration, light_adc_channel, &sample_mv);
        if (status != ESP_OK) {
            ESP_LOGE(log_tag, "light sensor read failed: %s", esp_err_to_name(status));
            return status;
        }
        sum_mv += sample_mv;
    }

    const float level_mv = static_cast<float>(sum_mv) / sample_count;
    lux = level_mv / millivolts_per_lux;
    return ESP_OK;
}
} // namespace bsp
