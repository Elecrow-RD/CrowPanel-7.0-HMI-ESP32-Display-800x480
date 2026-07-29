/*---------------------------------------------------------------
 * Touch input lesson
 * Read calibrated screen coordinates from the touch controller.
 *--------------------------------------------------------------*/

#include "touch.h"


/*---------------------------------------------------------------
 * Initialize touch input
 * Start diagnostics before configuring the selected controller.
 *--------------------------------------------------------------*/

/**
 * @brief Prepare serial output and the touch controller.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);
  touch_init();
}


/*---------------------------------------------------------------
 * Report touch coordinates
 * Print a position only when the controller reports an active touch.
 *--------------------------------------------------------------*/

/**
 * @brief Poll the touch controller and display valid coordinates.
 *
 * Separating signal availability from touch state allows the same lesson
 * to work with controller drivers that expose different interrupt models.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop() {
  if (touch_has_signal()) {
    if (touch_touched()) {
      Serial.print("Data x :");
      Serial.println(touch_last_x);
      Serial.print("Data y :");
      Serial.println(touch_last_y);
    }
  }
}
