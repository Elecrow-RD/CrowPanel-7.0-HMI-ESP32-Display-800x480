/*---------------------------------------------------------------
 * GT911 touch interface
 * Read capacitive touch coordinates and adapt them for LVGL.
 *--------------------------------------------------------------*/

#include <TAMC_GT911.h>
#include <Arduino_GFX_Library.h>

#define TOUCH_GT911
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_INT 15
#define TOUCH_GT911_RST 38
#define TOUCH_GT911_ROTATION ROTATION_NORMAL
#define TOUCH_MAP_X1 0
#define TOUCH_MAP_X2 800
#define TOUCH_MAP_Y1 0
#define TOUCH_MAP_Y2 480

// GT911 driver object configured for the 800 by 480 capacitive panel.
TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

// Latest calibrated touch position in display coordinates.
int touch_last_x = 0, touch_last_y = 0;

/**
 * @brief Initialize the GT911 bus and apply the selected rotation.
 * @param None.
 * @return Nothing.
 * @note Called once before LVGL begins polling the input device.
 */
void touch_init()
{
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  ts.begin();
  ts.setRotation(TOUCH_GT911_ROTATION);
}

/**
 * @brief Report whether the controller should be sampled.
 * @param None.
 * @return true because GT911 is polled over I2C in this lesson.
 * @note Called at the start of each LVGL input cycle.
 */
bool touch_has_signal()
{
  return true;
}

/**
 * @brief Read the first active GT911 point and map it to the panel.
 * @param None.
 * @return true when the controller reports an active contact.
 * @return false when the panel is not touched.
 * @note Called by the LVGL input callback.
 */
bool touch_touched()
{
  ts.read();
  if (ts.isTouched)
  {
    touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, lcd->width() - 1);
    touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, lcd->height() - 1);
    // Rotate the raw GT911 coordinates to match the displayed UI direction.
    touch_last_x = lcd->width() - 1 - touch_last_x;
    touch_last_y = lcd->height() - 1 - touch_last_y;
    Serial.printf("[TOUCH] x=%d y=%d\n", touch_last_x, touch_last_y);
    return true;
  }
  return false;
}

/**
 * @brief Report the release state through the common touch API.
 * @param None.
 * @return true when the controller is not reporting a pressed point.
 * @note Called when LVGL requests a sample without an active contact.
 */
bool touch_released()
{
  return true;
}
