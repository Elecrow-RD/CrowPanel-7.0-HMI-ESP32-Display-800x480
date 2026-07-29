/*---------------------------------------------------------------
 * GPS display lesson
 * Parse NMEA data and present live positioning information with LVGL.
 *--------------------------------------------------------------*/

#include <PCA9557.h>
#include <lvgl.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#define TFT_BL 2

/*---------------------------------------------------------------
 * GPS serial interface
 * Use the ESP32's second hardware UART for the external receiver.
 *--------------------------------------------------------------*/

#define GPS_RX 44
#define GPS_TX 43
// Receives the continuous NMEA character stream from the GPS module.
HardwareSerial gpsSerial(1);

/*---------------------------------------------------------------
 * NMEA receive and navigation state
 * Assemble one sentence at a time and retain the latest decoded values.
 *--------------------------------------------------------------*/

// Holds the NMEA sentence currently being collected from the UART.
char nmeaLine[128];
// Identifies the next free location in nmeaLine.
byte nmeaIndex = 0;

// Stores the most recent values assembled from GGA, RMC, and VTG sentences.
struct {
  bool valid = false;
  float lat = 0;
  float lon = 0;
  char latDir = 'N';
  char lonDir = 'E';
  float alt = 0;
  float speed = 0;
  uint8_t sats = 0;
  uint8_t fixType = 0;
  char timeStr[10] = "--:--:--";
  char dateStr[12] = "----/--/--";
} gps;

/*---------------------------------------------------------------
 * RGB display hardware description
 * Bind the board's parallel data pins and timing to LovyanGFX.
 *--------------------------------------------------------------*/

class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;

  /**
   * @brief Configure the RGB bus and its 800 by 480 panel.
   *
   * The pin order and porch timing must match the physical panel so each
   * frame is transferred with the correct color and synchronization signals.
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

/*---------------------------------------------------------------
 * LVGL display and input bridge
 * Connect LVGL rendering and pointer data to the board drivers.
 *--------------------------------------------------------------*/

// Logical dimensions shared by LVGL and the physical panel.
static constexpr uint32_t screenWidth = 800;
static constexpr uint32_t screenHeight = 480;
#include "touch.h"

/**
 * @brief Present a completed LVGL frame on the RGB display.
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

/**
 * @brief Translate controller state into an LVGL pointer sample.
 *
 * A press includes its latest calibrated position. Every path without an
 * active touch reports release so LVGL cannot retain a stale pressed state.
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


/*---------------------------------------------------------------
 * Validate NMEA sentences
 * Reject incomplete or corrupted input before modifying navigation state.
 *--------------------------------------------------------------*/

/**
 * @brief Verify the XOR checksum appended to an NMEA sentence.
 *
 * NMEA checksums cover the characters between '$' and '*'. A malformed
 * checksum suffix is rejected before its hexadecimal value is examined.
 *
 * @param line Null-terminated NMEA sentence to verify.
 * @return true when the calculated and received checksums match.
 * @return false when the sentence is malformed or corrupted.
 * @note Called by handleNMEA() for every completed input sentence.
 */
bool checkNMEA(const char* line) {
  const char* star = strchr(line, '*');
  if (!star || strlen(star) < 3) return false;
  byte calc = 0;
  for (const char* p = line + 1; *p && *p != '*'; p++) {
    calc ^= *p;
  }
  byte recv = (byte)strtol(star + 1, NULL, 16);
  return calc == recv;
}

/**
 * @brief Convert an NMEA degree-minute coordinate to decimal degrees.
 *
 * South and west coordinates become negative so the result follows the
 * conventional signed latitude and longitude representation.
 *
 * @param dm Coordinate encoded as degrees followed by decimal minutes.
 * @param dir Hemisphere letter: N, S, E, or W.
 * @return Signed coordinate in decimal degrees, or zero for invalid input.
 * @note Called while parsing valid GGA and RMC positions.
 */
float dmToDd(const char* dm, char dir) {
  if (!dm || strlen(dm) < 3) return 0;
  float val = atof(dm);
  int deg = (int)(val / 100);
  float min = val - deg * 100;
  float dd = deg + min / 60.0;
  return (dir == 'S' || dir == 'W') ? -dd : dd;
}

/*---------------------------------------------------------------
 * Decode supported NMEA sentence types
 * Extract complementary fields from GGA, RMC, and VTG messages.
 *--------------------------------------------------------------*/

/**
 * @brief Decode fix quality, satellites, altitude, time, and position.
 *
 * strtok() advances through fields in standard GGA order. Coordinates are
 * committed only when the receiver reports a valid fix quality.
 *
 * @param p Writable GGA sentence; tokenization modifies this buffer.
 * @return Nothing.
 * @note Called by handleNMEA() for GP and GN GGA sentences.
 */
void parseGGA(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // time
  if (tok && strlen(tok) >= 6) {
    snprintf(gps.timeStr, sizeof(gps.timeStr), "%c%c:%c%c:%c%c",
             tok[0], tok[1], tok[2], tok[3], tok[4], tok[5]);
  }
  tok = strtok(NULL, ","); // lat
  char* lat = tok;
  tok = strtok(NULL, ","); // N/S
  char latD = tok ? tok[0] : 'N';
  tok = strtok(NULL, ","); // lon
  char* lon = tok;
  tok = strtok(NULL, ","); // E/W
  char lonD = tok ? tok[0] : 'E';
  tok = strtok(NULL, ","); // fix
  gps.fixType = tok ? atoi(tok) : 0;
  gps.valid = (gps.fixType > 0);
  tok = strtok(NULL, ","); // sats
  gps.sats = tok ? atoi(tok) : 0;
  tok = strtok(NULL, ","); // hdop
  tok = strtok(NULL, ","); // alt
  gps.alt = (tok && strlen(tok) > 0) ? atof(tok) : 0;
  
  if (gps.valid) {
    gps.lat = dmToDd(lat, latD);
    gps.lon = dmToDd(lon, lonD);
    gps.latDir = latD;
    gps.lonDir = lonD;
  }
}

/**
 * @brief Decode validity, speed, date, and position from an RMC sentence.
 *
 * RMC reports speed in knots, so multiplying by 1.852 converts it to the
 * kilometres-per-hour unit displayed by the interface.
 *
 * @param p Writable RMC sentence; tokenization modifies this buffer.
 * @return Nothing.
 * @note Called by handleNMEA() for GP and GN RMC sentences.
 */
void parseRMC(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // time
  tok = strtok(NULL, ","); // status
  gps.valid = (tok && tok[0] == 'A');
  tok = strtok(NULL, ","); // lat
  char* lat = tok;
  tok = strtok(NULL, ","); // N/S
  char latD = tok ? tok[0] : 'N';
  tok = strtok(NULL, ","); // lon
  char* lon = tok;
  tok = strtok(NULL, ","); // E/W
  char lonD = tok ? tok[0] : 'E';
  tok = strtok(NULL, ","); // speed knots
  gps.speed = (tok && strlen(tok) > 0) ? atof(tok) * 1.852 : 0;
  tok = strtok(NULL, ","); // course
  tok = strtok(NULL, ","); // date
  if (tok && strlen(tok) == 6) {
    snprintf(gps.dateStr, sizeof(gps.dateStr), "20%c%c/%c%c/%c%c",
             tok[4], tok[5], tok[2], tok[3], tok[0], tok[1]);
  }
  
  if (gps.valid) {
    gps.lat = dmToDd(lat, latD);
    gps.lon = dmToDd(lon, lonD);
    gps.latDir = latD;
    gps.lonDir = lonD;
  }
}

/**
 * @brief Decode the kilometres-per-hour field from a VTG sentence.
 *
 * @param p Writable VTG sentence; tokenization modifies this buffer.
 * @return Nothing.
 * @note Called by handleNMEA() for GP and GN VTG sentences.
 */
void parseVTG(char* p) {
  char* tok = strtok(p, ",");
  tok = strtok(NULL, ","); // true track
  tok = strtok(NULL, ","); // T
  tok = strtok(NULL, ","); // mag track
  tok = strtok(NULL, ","); // M
  tok = strtok(NULL, ","); // speed knots
  tok = strtok(NULL, ","); // N
  tok = strtok(NULL, ","); // speed km/h
  if (tok && strlen(tok) > 0) {
    gps.speed = atof(tok);
  }
}

/**
 * @brief Validate and dispatch the assembled NMEA sentence.
 *
 * Both GP and GN talker prefixes are accepted because receivers may emit
 * GPS-only or combined-constellation messages with the same field layout.
 *
 * @param None.
 * @return Nothing.
 * @note Called from loop() when a line-ending character completes a sentence.
 */
void handleNMEA() {
  if (nmeaIndex < 10) return;
  nmeaLine[nmeaIndex] = '\0';
  
  if (!checkNMEA(nmeaLine)) return;
  
  if (strncmp(nmeaLine, "$GPGGA", 6) == 0 || strncmp(nmeaLine, "$GNGGA", 6) == 0) {
    parseGGA(nmeaLine);
  }
  else if (strncmp(nmeaLine, "$GPRMC", 6) == 0 || strncmp(nmeaLine, "$GNRMC", 6) == 0) {
    parseRMC(nmeaLine);
  }
  else if (strncmp(nmeaLine, "$GPVTG", 6) == 0 || strncmp(nmeaLine, "$GNVTG", 6) == 0) {
    parseVTG(nmeaLine);
  }
}


/*---------------------------------------------------------------
 * GPS user-interface objects
 * Retain labels whose content or visibility changes during operation.
 *--------------------------------------------------------------*/

// Status and measurement labels updated by updateGpsDisplay().
lv_obj_t* labelStatus;
lv_obj_t* labelTime;
lv_obj_t* labelLat;
lv_obj_t* labelLon;
lv_obj_t* labelAlt;
lv_obj_t* labelSpeed;
lv_obj_t* labelSat;
lv_obj_t* labelDate;

/**
 * @brief Construct the static GPS dashboard and its dynamic labels.
 *
 * Measurement labels begin hidden because the receiver may not yet have a
 * valid fix. updateGpsDisplay() selects the appropriate presentation later.
 *
 * @param None.
 * @return Nothing.
 * @note Called once from setup() after LVGL display registration.
 */
void createGpsUI()
{
  // The screen is divided into a persistent status bar and a data region.
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);
  
  // Status bar (top colored bar)
  lv_obj_t* statusBar = lv_obj_create(lv_screen_active());
  lv_obj_set_size(statusBar, 800, 50);
  lv_obj_align(statusBar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x1B5E), 0); // Default green
  lv_obj_set_style_radius(statusBar, 0, 0);
  lv_obj_set_style_border_width(statusBar, 0, 0);
  
  // Status text
  labelStatus = lv_label_create(statusBar);
  lv_label_set_text(labelStatus, "  GPS LOCKED");
  lv_obj_set_style_text_color(labelStatus, lv_color_white(), 0);
  lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_24, 0);
  lv_obj_align(labelStatus, LV_ALIGN_LEFT_MID, 10, 0);
  
  // Time
  labelTime = lv_label_create(statusBar);
  lv_label_set_text(labelTime, "--:--:--");
  lv_obj_set_style_text_color(labelTime, lv_color_white(), 0);
  lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_16, 0);
  lv_obj_align(labelTime, LV_ALIGN_RIGHT_MID, -20, 0);
  
  // This central prompt is used only while no valid position is available.
  labelSat = lv_label_create(lv_screen_active());
  lv_label_set_text(labelSat, "Acquiring...");
  lv_obj_set_style_text_color(labelSat, lv_color_hex(0xC000), 0);
  lv_obj_set_style_text_font(labelSat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelSat, LV_ALIGN_CENTER, 0, -60);
  lv_obj_add_flag(labelSat, LV_OBJ_FLAG_HIDDEN); // Hidden by default
  
  // Valid-fix measurements occupy fixed positions in the data region.
  // Latitude (large font)
  labelLat = lv_label_create(lv_screen_active());
  lv_label_set_text(labelLat, "0.00000");
  lv_obj_set_style_text_color(labelLat, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLat, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLat, LV_ALIGN_TOP_LEFT, 30, 80);
  lv_obj_add_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
  
  // Longitude (large font)
  labelLon = lv_label_create(lv_screen_active());
  lv_label_set_text(labelLon, "0.00000");
  lv_obj_set_style_text_color(labelLon, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelLon, &lv_font_montserrat_36, 0);
  lv_obj_align(labelLon, LV_ALIGN_TOP_LEFT, 30, 140);
  lv_obj_add_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
  
  // Divider line
  lv_obj_t* line = lv_line_create(lv_screen_active());
  static lv_point_precise_t line_points[] = {{30, 200}, {400, 200}};
  lv_line_set_points(line, line_points, 2);
  lv_obj_set_style_line_color(line, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_style_line_width(line, 2, 0);
  
  // Altitude
  labelAlt = lv_label_create(lv_screen_active());
  lv_label_set_text(labelAlt, "ALT  0.0 m");
  lv_obj_set_style_text_color(labelAlt, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelAlt, &lv_font_montserrat_24, 0);
  lv_obj_align(labelAlt, LV_ALIGN_TOP_LEFT, 30, 220);
  lv_obj_add_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
  
  // Speed
  labelSpeed = lv_label_create(lv_screen_active());
  lv_label_set_text(labelSpeed, "SPD  0.0 km/h");
  lv_obj_set_style_text_color(labelSpeed, lv_color_black(), 0);
  lv_obj_set_style_text_font(labelSpeed, &lv_font_montserrat_24, 0);
  lv_obj_align(labelSpeed, LV_ALIGN_TOP_LEFT, 30, 260);
  lv_obj_add_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
  
  // Satellites label
  lv_obj_t* satLabel = lv_label_create(lv_screen_active());
  lv_label_set_text(satLabel, "SAT");
  lv_obj_set_style_text_color(satLabel, lv_color_black(), 0);
  lv_obj_set_style_text_font(satLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(satLabel, LV_ALIGN_TOP_LEFT, 30, 300);
  
  // Date
  labelDate = lv_label_create(lv_screen_active());
  lv_label_set_text(labelDate, "----/--/--");
  lv_obj_set_style_text_color(labelDate, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_16, 0);
  lv_obj_align(labelDate, LV_ALIGN_TOP_LEFT, 200, 310);
  lv_obj_add_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
}

/*---------------------------------------------------------------
 * Refresh the GPS dashboard
 * Switch between acquisition and locked modes, then update live values.
 *--------------------------------------------------------------*/

/**
 * @brief Apply the latest navigation state to all dynamic UI objects.
 *
 * Visibility changes occur only when fix validity changes. Text values are
 * then refreshed for the active mode, avoiding meaningless measurements
 * while the receiver is still acquiring satellites.
 *
 * @param None.
 * @return Nothing.
 * @note Called from loop() every 800 milliseconds.
 */
void updateGpsDisplay()
{
  static bool lastValid = false;
  char buf[48];
  
  // A validity transition changes which group of labels is visible.
  if (gps.valid != lastValid)
  {
    if (gps.valid)
    {
      // A valid fix reveals measurements and removes the waiting prompt.
      lv_obj_add_flag(labelSat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      // Loss of the fix hides stale measurements and restores acquisition status.
      lv_obj_clear_flag(labelSat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelLat, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelLon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelAlt, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelSpeed, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(labelDate, LV_OBJ_FLAG_HIDDEN);
    }
    lastValid = gps.valid;
  }
  
  // Color provides an immediate visual distinction between locked and searching.
  lv_obj_t* statusBar = lv_obj_get_parent(labelStatus);
  if (gps.valid) {
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x1B5E), 0); // Green
    lv_label_set_text(labelStatus, "  GPS LOCKED");
  } else {
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(0xC000), 0); // Red
    lv_label_set_text(labelStatus, "  NO SIGNAL");
  }
  
  lv_label_set_text(labelTime, gps.timeStr);
  
  if (!gps.valid)
  {
    // Satellite count remains useful feedback even before a position is valid.
    snprintf(buf, sizeof(buf), "Satellites: %d", gps.sats);
    lv_label_set_text(labelSat, buf);
    return;
  }
  
  // Only a valid fix is allowed to populate the measurement labels.
  snprintf(buf, sizeof(buf), "%.5f", gps.lat);
  lv_label_set_text(labelLat, buf);
  
  snprintf(buf, sizeof(buf), "%.5f", gps.lon);
  lv_label_set_text(labelLon, buf);
  
  snprintf(buf, sizeof(buf), "ALT  %.1f m", gps.alt);
  lv_label_set_text(labelAlt, buf);
  
  snprintf(buf, sizeof(buf), "SPD  %.1f km/h", gps.speed);
  lv_label_set_text(labelSpeed, buf);
  
  lv_label_set_text(labelDate, gps.dateStr);
}


/*---------------------------------------------------------------
 * Hardware initialization
 * Bring up shared I2C devices, display, touch, LVGL, and the GPS UART.
 *--------------------------------------------------------------*/

// Controls board-level reset and support signals through the I2C expander.
PCA9557 Out;

/**
 * @brief Initialize every hardware and software layer used by the lesson.
 *
 * The RGB frame buffers are obtained from LovyanGFX and registered directly
 * with LVGL in full-render mode, avoiding an additional copy per frame.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup()
{
  Serial.begin(115200);
  
  // The expander supplies the reset sequence required before display startup.
  Wire.begin(19, 20);
  Out.reset();
  Out.setMode(IO_OUTPUT);  
  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);
  Serial.println("PCA9557 init done");

  // Hold the board control output low while the display is initialized.
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

  lv_color_t* frameBuffer0 = (lv_color_t*)lcd._bus_instance.getFrameBuffer(0);
  lv_color_t* frameBuffer1 = (lv_color_t*)lcd._bus_instance.getFrameBuffer(1);
  Serial.printf("LovyanGFX RGB buffers: %p, %p, free PSRAM: %u\n",
                frameBuffer0, frameBuffer1, ESP.getFreePsram());
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
  // PWM first raises the backlight to full brightness.
  //digitalWrite(TFT_BL, HIGH);
  ledcAttach(TFT_BL, 300, 8);   
  ledcWrite(TFT_BL, 255);       
#endif
 
#ifdef TFT_BL
  // The following explicit transition preserves the board's startup sequence.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); 
  delay(500);
  digitalWrite(TFT_BL, HIGH);
#endif

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  createGpsUI();
  
  // A temporary message covers the dashboard during initial acquisition.
  lv_obj_t* startup = lv_label_create(lv_screen_active());
  lv_label_set_text(startup, "GPS Display\nWaiting for satellites...");
  lv_obj_set_style_text_color(startup, lv_color_black(), 0);
  lv_obj_set_style_text_font(startup, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(startup, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(startup, LV_ALIGN_CENTER, 0, 0);
  
  lv_timer_handler();
  delay(1000);
  lv_obj_delete(startup);
  
  Serial.println("--- GPS Display ready ---");
}

/*---------------------------------------------------------------
 * Runtime data flow
 * Assemble UART lines, refresh the UI periodically, and service LVGL.
 *--------------------------------------------------------------*/

/**
 * @brief Process incoming GPS data and maintain the graphical interface.
 *
 * Line endings delimit NMEA sentences. A bounds check prevents a sentence
 * from writing past nmeaLine, while UI refresh timing remains independent
 * of the rate at which individual serial characters arrive.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop()
{
  // A complete buffered line is parsed before collection restarts at index zero.
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (c == '\n' || c == '\r') {
      if (nmeaIndex > 0) {
        handleNMEA();
        nmeaIndex = 0;
      }
    } else if (nmeaIndex < sizeof(nmeaLine) - 1) {
      nmeaLine[nmeaIndex++] = c;
    }
  }
  
  // Throttling text updates leaves LVGL time to render and process input smoothly.
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 800) {
    updateGpsDisplay();
    lastDraw = millis();
  }
  
  lv_timer_handler();
  delay(5);
}
