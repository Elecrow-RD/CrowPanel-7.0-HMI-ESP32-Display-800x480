/*---------------------------------------------------------------
 * PlatformIO LVGL dashboard
 * Drive the 800 x 480 RGB panel, GT911 touch input, and DHT20 sensor.
 *--------------------------------------------------------------*/

#include <DHT20.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lvgl.h>

#include <climits>

#include "ui.h"

/*---------------------------------------------------------------
 * RGB display configuration
 * Bind the ESP32-S3 RGB signals and panel timing to LovyanGFX.
 *--------------------------------------------------------------*/

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB bus;
    lgfx::Panel_RGB panel;

    /**
     * @brief Configure the RGB bus and the 800 by 480 panel.
     * @param None.
     * @return A configured LGFX device through the global lcd object.
     * @note Runs automatically while lcd is constructed before setup().
     */
    LGFX()
    {
        auto bus_config = bus.config();
        bus_config.panel = &panel;
        bus_config.pin_d0 = GPIO_NUM_15;
        bus_config.pin_d1 = GPIO_NUM_7;
        bus_config.pin_d2 = GPIO_NUM_6;
        bus_config.pin_d3 = GPIO_NUM_5;
        bus_config.pin_d4 = GPIO_NUM_4;
        bus_config.pin_d5 = GPIO_NUM_9;
        bus_config.pin_d6 = GPIO_NUM_46;
        bus_config.pin_d7 = GPIO_NUM_3;
        bus_config.pin_d8 = GPIO_NUM_8;
        bus_config.pin_d9 = GPIO_NUM_16;
        bus_config.pin_d10 = GPIO_NUM_1;
        bus_config.pin_d11 = GPIO_NUM_14;
        bus_config.pin_d12 = GPIO_NUM_21;
        bus_config.pin_d13 = GPIO_NUM_47;
        bus_config.pin_d14 = GPIO_NUM_48;
        bus_config.pin_d15 = GPIO_NUM_45;
        bus_config.pin_henable = GPIO_NUM_41;
        bus_config.pin_vsync = GPIO_NUM_40;
        bus_config.pin_hsync = GPIO_NUM_39;
        bus_config.pin_pclk = GPIO_NUM_0;
        bus_config.freq_write = 24000000;
        bus_config.hsync_polarity = 0;
        bus_config.hsync_front_porch = 40;
        bus_config.hsync_pulse_width = 48;
        bus_config.hsync_back_porch = 40;
        bus_config.vsync_polarity = 0;
        bus_config.vsync_front_porch = 1;
        bus_config.vsync_pulse_width = 31;
        bus_config.vsync_back_porch = 13;
        bus_config.pclk_active_neg = 1;
        bus_config.de_idle_high = 0;
        bus_config.pclk_idle_high = 0;
        bus.config(bus_config);

        auto panel_config = panel.config();
        panel_config.memory_width = 800;
        panel_config.memory_height = 480;
        panel_config.panel_width = 800;
        panel_config.panel_height = 480;
        panel_config.offset_x = 0;
        panel_config.offset_y = 0;
        panel.config(panel_config);

        panel.setBus(&bus);
        setPanel(&panel);
    }

};

// Owns the panel and the two full-screen frame buffers used by LVGL.
LGFX lcd;

#include "touch.h"

namespace
{
// Board pins and logical dimensions shared by the display pipeline.
constexpr uint8_t   BACKLIGHT_PIN = 2;
constexpr uint8_t   RELAY_PIN = 38;
constexpr uint32_t  SCREEN_WIDTH = 800;
constexpr uint32_t  SCREEN_HEIGHT = 480;

DHT20 dht20(&Wire1);

/**
 * @brief Return milliseconds for LVGL's internal time base.
 * @param None.
 * @return Current Arduino millisecond counter.
 * @note Registered with LVGL during setup().
 */
uint32_t tick_get()
{
    return millis();
}

/**
 * @brief Switch the RGB panel to the buffer rendered by LVGL.
 * @param display LVGL display requesting the transfer.
 * @param area Unused because full-frame rendering is configured.
 * @param pixel_map Frame buffer selected by LVGL.
 * @return Nothing.
 * @note Called by LVGL after each rendered frame.
 */
void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    (void)area;
    if(!lcd.bus.presentFrameBuffer(pixel_map)) {
        Serial.println("LovyanGFX VSYNC frame switch timeout");
    }
    lv_display_flush_ready(display);
}

/**
 * @brief Supply the current touch state and coordinates to LVGL.
 * @param indev LVGL input device requesting a sample.
 * @param data Destination for pointer state and coordinates.
 * @return Nothing.
 * @note Called periodically after the input device is registered.
 */
void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;

    if(touch_touched()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
    }
}
}

// Desired output state selected by the generated UI button callbacks.
int led = 0;

/*---------------------------------------------------------------
 * Hardware and UI initialization
 * Start buses, peripherals, LVGL, and the generated dashboard.
 *--------------------------------------------------------------*/

/**
 * @brief Initialize the display, touch controller, sensor, and UI.
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup()
{
    Serial.begin(9600);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    Wire1.begin(19, 20);
    Wire1.setClock(400000);

    if(!lcd.begin()) {
        Serial.println("lcd.begin() failed");
        return;
    }
    delay(200);

    Wire1.end();
    delay(10);
    Wire1.begin(19, 20);
    Wire1.setClock(400000);

    lv_init();
    lv_tick_set_cb(tick_get);
    touch_init();
    const bool dht_connected = dht20.begin();
    Serial.printf("DHT20: %s\n", dht_connected ? "connected" : "not found");

    lv_color_t *frame_buffer_0 = reinterpret_cast<lv_color_t *>(lcd.bus.getFrameBuffer(0));
    lv_color_t *frame_buffer_1 = reinterpret_cast<lv_color_t *>(lcd.bus.getFrameBuffer(1));
    Serial.printf("RGB frame buffers: %p, %p\n", frame_buffer_0, frame_buffer_1);
    if(frame_buffer_0 == nullptr || frame_buffer_1 == nullptr) {
        Serial.println("RGB double frame buffer allocation failed");
        return;
    }

    lv_display_t *display = lv_display_create(lcd.width(), lcd.height());
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_buffers(display, frame_buffer_0, frame_buffer_1,
                           SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *touchpad = lv_indev_create();
    lv_indev_set_type(touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touchpad, touchpad_read);

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);

    ui_init();
}

/*---------------------------------------------------------------
 * Runtime sensor and UI processing
 * Read the DHT20 at a safe interval and service LVGL continuously.
 *--------------------------------------------------------------*/

/**
 * @brief Refresh sensor labels, output state, and LVGL timers.
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop()
{
    static uint32_t last_sensor_update = 0;
    static int previous_temperature = INT_MIN;
    static int previous_humidity = INT_MIN;

    const uint32_t now = millis();

    // Read the sensor on a fixed interval so the UI remains responsive.
    if(now - last_sensor_update >= 2000) {
        last_sensor_update = now;

        const int status = dht20.read();
        if(status == DHT20_OK) {
            const int temperature = lroundf(dht20.getTemperature());
            const int humidity = lroundf(dht20.getHumidity());
            char sensor_text[6];

            if(temperature != previous_temperature) {
                snprintf(sensor_text, sizeof(sensor_text), "%d", temperature);
                lv_label_set_text(ui_TempLabel, sensor_text);
                previous_temperature = temperature;
            }

            if(humidity != previous_humidity) {
                snprintf(sensor_text, sizeof(sensor_text), "%d", humidity);
                lv_label_set_text(ui_HumiLabel, sensor_text);
                previous_humidity = humidity;
            }
        }
        else {
            Serial.printf("DHT20 read error: %d\n", status);
        }
    }

    // The generated UI event callbacks update led; the loop applies it to GPIO38.
    digitalWrite(RELAY_PIN, led == 1 ? HIGH : LOW);

    lv_timer_handler();
    delay(10);
}
