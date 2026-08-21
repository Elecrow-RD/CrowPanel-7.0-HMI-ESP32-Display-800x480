/*******************************************************************************
 * Touch libraries:
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 ******************************************************************************/

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

TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

int touch_last_x = 0, touch_last_y = 0;

void touch_init()
{
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  ts.begin();
  ts.setRotation(TOUCH_GT911_ROTATION);
}

bool touch_has_signal()
{
  return true;
}

bool touch_touched()
{
  ts.read();
  if (ts.isTouched)
  {
    touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, lcd->width() - 1);
    touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, lcd->height() - 1);
    // 180° rotation compensation for GT911
    touch_last_x = lcd->width() - 1 - touch_last_x;
    touch_last_y = lcd->height() - 1 - touch_last_y;
    Serial.printf("[TOUCH] x=%d y=%d\n", touch_last_x, touch_last_y);
    return true;
  }
  return false;
}

bool touch_released()
{
  return true;
}