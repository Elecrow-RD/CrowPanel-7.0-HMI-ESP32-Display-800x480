/*---------------------------------------------------------------
 * LVGL environmental dashboard lesson
 * Display temperature and humidity, then control an output by touch.
 *--------------------------------------------------------------*/

#include <PCA9557.h>
#include <lvgl.h>
#include <Crowbits_DHT20.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <SPI.h>
#include <climits>
#include <esp_heap_caps.h>
#include "ui.h"

#define TFT_BL 2

/*---------------------------------------------------------------
 * RGB display hardware description
 * Bind the board's parallel signals and timing to LovyanGFX.
 *--------------------------------------------------------------*/

class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;

  /**
   * @brief Configure the RGB bus and its 800 by 480 panel.
   *
   * The data-pin order and synchronization timing must match the physical
   * panel so the display receives correctly aligned RGB565 frames.
   *
   * @param None.
   * @return A configured LGFX display object.
   * @note Called automatically while the global lcd object is constructed.
   */
  LGFX()
  {
    auto busConfig = _bus_instance.config();
    busConfig.panel = &_panel_instance;
    const int8_t dataPins[16] = {15, 7, 6, 5, 4, 9, 46, 3, 8, 16, 1, 14, 21, 47, 48, 45};
    memcpy(busConfig.pin_data, dataPins, sizeof(dataPins));
    busConfig.pin_henable = 41;
    busConfig.pin_vsync = 40;
    busConfig.pin_hsync = 39;
    busConfig.pin_pclk = 0;
    busConfig.freq_write = 24000000;
    busConfig.hsync_polarity = 0;
    busConfig.hsync_front_porch = 40;
    busConfig.hsync_pulse_width = 48;
    busConfig.hsync_back_porch = 40;
    busConfig.vsync_polarity = 0;
    busConfig.vsync_front_porch = 1;
    busConfig.vsync_pulse_width = 31;
    busConfig.vsync_back_porch = 13;
    busConfig.pclk_active_neg = 1;
    busConfig.de_idle_high = 0;
    busConfig.pclk_idle_high = 0;
    _bus_instance.config(busConfig);

    auto panelConfig = _panel_instance.config();
    panelConfig.memory_width = 800;
    panelConfig.memory_height = 480;
    panelConfig.panel_width = 800;
    panelConfig.panel_height = 480;
    _panel_instance.config(panelConfig);
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};

// Owns the RGB bus, panel, and frame buffers used for drawing.
LGFX lcd;

// Logical dimensions shared by LVGL and the physical panel.
static constexpr uint32_t screenWidth = 800;
static constexpr uint32_t screenHeight = 480;

// Desired state of the output controlled by the two UI buttons.
int led;

// Provides temperature and humidity measurements from the DHT20 sensor.
Crowbits_DHT20 dht20;

// Retains a reference to the board's default SPI peripheral.
SPIClass& spi = SPI;

#include "touch.h"

/*---------------------------------------------------------------
 * LVGL display bridge
 * Present LVGL's completed frame through the LovyanGFX RGB bus.
 *--------------------------------------------------------------*/

/**
 * @brief Switch the RGB panel to the frame buffer rendered by LVGL.
 *
 * @param display LVGL display that requested the transfer.
 * @param area Updated area supplied by LVGL; full-frame mode is used here.
 * @param pixelMap Address of the frame buffer ready for presentation.
 * @return Nothing.
 * @note Called by LVGL whenever rendering for a frame is complete.
 */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap)
{
  if (!lcd._bus_instance.presentFrameBuffer(pixelMap)) {
    Serial.println("LovyanGFX VSYNC frame switch timeout");
  }
  lv_display_flush_ready(display);
}

/*---------------------------------------------------------------
 * LVGL touch bridge
 * Convert the board touch driver's state into LVGL pointer samples.
 *--------------------------------------------------------------*/

/**
 * @brief Supply the current touch state and position to LVGL.
 *
 * A press includes calibrated coordinates. Paths without an active touch
 * explicitly report release so LVGL cannot retain a stale pressed state.
 *
 * @param indev LVGL input device requesting a sample.
 * @param data Destination for the pointer state and coordinates.
 * @return Nothing.
 * @note Called periodically by LVGL after registration in setup().
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print("Data x ");
      Serial.println(data->point.x);
      Serial.print("Data y ");
      Serial.println(data->point.y);
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
  delay(15);
}

// Controls board-level reset and support signals through the I2C expander.
PCA9557 Out;

/*---------------------------------------------------------------
 * Initialize the dashboard
 * Bring up hardware in dependency order before loading the generated UI.
 *--------------------------------------------------------------*/

/**
 * @brief Initialize the display, touch input, sensor, LVGL, and interface.
 *
 * The RGB frame buffers owned by LovyanGFX are registered directly with
 * LVGL in full-render mode, avoiding an additional full-screen copy.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("LVGL_Arduino7.0 starting...");
  Serial.printf("PSRAM: %s, size: %u, free: %u\n",
                psramFound() ? "OK" : "NOT FOUND",
                ESP.getPsramSize(), ESP.getFreePsram());

  Wire.begin(19, 20);

  // The panel controller must leave reset before the RGB bus starts.
  Out.reset();
  Out.setMode(IO_OUTPUT);
  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);
  Serial.println("PCA9557 init done");

  dht20.begin();

  // Hold the controlled output low until the interface supplies a command.
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);

  if (!lcd.begin()) {
    Serial.println("lcd.begin() failed!");
    Serial.println("Check Arduino Tools > PSRAM is set to OPI PSRAM.");
    return;
  } else {
    Serial.println("LovyanGFX lcd.begin() OK");
  }
  delay(200);

  lv_init();
  lv_tick_set_cb(millis);
  delay(100);

  touch_init();
  Serial.println("touch init done");

  Serial.printf("screen: %u x %u\n", screenWidth, screenHeight);

  // LovyanGFX owns two full-screen RGB buffers after lcd.begin() succeeds.
  lv_color_t* frameBuffer0 = (lv_color_t*)lcd._bus_instance.getFrameBuffer(0);
  lv_color_t* frameBuffer1 = (lv_color_t*)lcd._bus_instance.getFrameBuffer(1);
  Serial.printf("LovyanGFX RGB buffers: %p, %p, free PSRAM: %u\n",
                frameBuffer0, frameBuffer1, ESP.getFreePsram());

  // LVGL renders directly into the LovyanGFX buffers and asks the flush
  // callback to present the completed frame at the next VSYNC boundary.
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, my_disp_flush);
  lv_display_set_buffers(display, frameBuffer0, frameBuffer1,
                         screenWidth * screenHeight * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_FULL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  ui_init();
  lv_timer_handler();

  Serial.println("Setup done");
}

/*---------------------------------------------------------------
 * Run the dashboard
 * Sample slowly changing sensor data while servicing LVGL continuously.
 *--------------------------------------------------------------*/

/**
 * @brief Refresh sensor labels, apply output state, and process LVGL tasks.
 *
 * Sensor reads are limited to once per second. Labels change only when the
 * integer display value changes, which avoids unnecessary LVGL redraws.
 * Button callbacks update led asynchronously through the UI event layer.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop()
{
  static uint32_t lastSensorRead = 0;
  static int lastTemperature = INT_MIN;
  static int lastHumidity = INT_MIN;
  uint32_t now = millis();

  // Unsigned elapsed-time comparison remains valid across millis() rollover.
  if (now - lastSensorRead >= 1000) {
    lastSensorRead = now;
    int temperature = (int)dht20.getTemperature();
    int humidity = (int)dht20.getHumidity();
    char DHT_buffer[6];
    if (temperature != lastTemperature) {
      lastTemperature = temperature;
      snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", temperature);
      lv_label_set_text(ui_TempLabel, DHT_buffer);
    }
    if (humidity != lastHumidity) {
      lastHumidity = humidity;
      snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", humidity);
      lv_label_set_text(ui_HumiLabel, DHT_buffer);
    }
  }

  // The two independent checks preserve the command selected by the UI.
  if (led == 1)
    digitalWrite(38, HIGH);
  if (led == 0)
    digitalWrite(38, LOW);

  lv_timer_handler();
  delay(10);
}
