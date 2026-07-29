#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

// Generated LVGL objects used by the dashboard.
#include "ui.h"
#include <SPI.h>

#include <DHT20.h>

// Display dimensions are read from the panel after initialization.
static uint32_t screenWidth;
static uint32_t screenHeight;

// Partial-render buffer shared by LVGL and the RGB display driver.
static lv_color_t *lvgl_draw_buf = nullptr;

#include <Arduino_GFX_Library.h>
#include <PCA9557.h>
// GPIO used to enable the LCD backlight.
#define TFT_BL 2

// Board-level I2C expander used by the optional reset sequence.
PCA9557 Out;

/*---------------------------------------------------------------
 * RGB display hardware description
 * Bind the ESP32-S3 RGB signals and panel timing to Arduino_GFX.
 *--------------------------------------------------------------*/

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 0 /* PCLK */,
    // R0~R4
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    // G0~G5
    9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
    // B0~B4
    15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */,
    0 /* hsync_polarity */, 40 /* hsync_front_porch */, 48 /* hsync_pulse_width */, 40 /* hsync_back_porch */,
    0 /* vsync_polarity */, 1 /* vsync_front_porch */, 31 /* vsync_pulse_width */, 13 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 16000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 800 * 10 /* bounce_buffer_size_px */
);

Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    800 /* width */, 480 /* height */, bus, 0 /* rotation */, false /* auto_flush */);

#include "touch.h"

// Temperature and humidity sensor connected to the board I2C bus.
DHT20 dht20;

// Desired output state requested by the generated UI button events.
int led = 0;

// Last output state applied to GPIO38; prevents repeated writes and logs.
static int last_led_state = 0;

/*---------------------------------------------------------------
 * Refresh sensor labels
 * Read the DHT20 and update only the generated labels.
 *--------------------------------------------------------------*/

/**
 * @brief Read the DHT20 and publish integer values to the LVGL labels.
 * @param None.
 * @return Nothing.
 * @note Called periodically from loop() after the sensor has been started.
 */
void update_sensor_values() {
  int temperature = dht20.getTemperature();
  int humidity = dht20.getHumidity();

  if (ui_TempLabel) lv_label_set_text_fmt(ui_TempLabel, "%d", temperature);
  if (ui_HumiLabel) lv_label_set_text_fmt(ui_HumiLabel, "%d", humidity);

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humi: ");
  Serial.print(humidity);
  Serial.println(" %");
}

/**
 * @brief Transfer one LVGL render area to the physical display.
 * @param display LVGL display requesting the transfer.
 * @param area Updated rectangle in display coordinates.
 * @param pixel_map RGB565 pixels produced by LVGL.
 * @return Nothing.
 * @note Called by LVGL whenever a render buffer is ready.
 */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pixel_map), w, h);

    lv_display_flush_ready(display);
}

/**
 * @brief Provide the current touch state to LVGL.
 * @param indev LVGL input device requesting a sample.
 * @param data Destination for state and coordinates.
 * @return Nothing.
 * @note Called periodically by LVGL after input registration in setup().
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            Serial.printf("[LVGL] x=%d y=%d\n", touch_last_x, touch_last_y);
        }
        else if (touch_released())
        {
            data->state = LV_INDEV_STATE_REL;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/**
 * @brief Initialize all peripherals and register them with LVGL.
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino-compatible ESP-IDF startup code.
 */
void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("setup() start");

    dht20.begin();

    // Some board revisions route LCD reset through PCA9557. This lesson keeps
    // the sequence documented but inactive because the target board revision
    // initializes correctly without driving those expander pins.
    /*
    Wire.begin(19, 20);
    Out.reset();
    Out.setMode(IO_OUTPUT);
    Out.setState(0, IO_LOW);
    Out.setState(1, IO_LOW);
    delay(20);
    Out.setState(0, IO_HIGH);
    delay(100);
    Out.setMode(1, IO_INPUT);
    */
    // The shared I2C bus is still required for the DHT20 sensor and GT911 touch.
    Wire.begin(19, 20);

    // Keep the controlled output low until a UI event requests a change.
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);

    lv_init();

    Serial.println("Calling lcd->begin()...");
    bool ok = lcd->begin();
    Serial.printf("lcd->begin() returned: %d\r\n", ok);
    if (!ok) {
        Serial.println("lcd->begin() failed, halt");
        while (1) {
            delay(1000);
        }
    }

    // Clear the panel before the generated UI is created.
    lcd->fillScreen(0x0000);
    delay(100);

    touch_init();
    delay(50);

    //pinMode(38, OUTPUT);
    //digitalWrite(38, LOW);

    screenWidth = lcd->width();
    screenHeight = lcd->height();

    // A 20-line DMA buffer lets LVGL render in small strips instead of
    // reserving a full 800 by 480 frame buffer in internal memory.
    const size_t draw_buf_pixels = screenWidth * 20;
    lvgl_draw_buf = (lv_color_t *)heap_caps_malloc(draw_buf_pixels * sizeof(lv_color_t),
                                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!lvgl_draw_buf) {
        Serial.println("draw buffer alloc failed");
        while (1) {
            delay(1000);
        }
    }
    lv_display_t *display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_buffers(display, lvgl_draw_buf, NULL,
                           draw_buf_pixels * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    lv_indev_set_display(indev, display);

    ui_init();

    // Enable the backlight only after LVGL and the UI are ready.
#ifdef TFT_BL
    ledcAttach(TFT_BL, 300, 8);
    ledcWrite(TFT_BL, 255);
#endif
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    delay(500);
    digitalWrite(TFT_BL, HIGH);

    Serial.println("Setup done");
}

void loop()
{
    static uint32_t last_tick_ms = millis();

    // Advance LVGL's time base before servicing animations and input.
    uint32_t now = millis();
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;

    lv_timer_handler();
    delay(5);

    // Apply button commands only when the generated UI changes the state.
    if (led != last_led_state) {
        last_led_state = led;
        const int LED_PIN = 38;
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, led ? HIGH : LOW);
        Serial.printf("LED state changed: %d\n", led);
    }

    static unsigned long last_update = 0;
    if (millis() - last_update > 2000) {
        update_sensor_values();
        last_update = millis();
    }
}
