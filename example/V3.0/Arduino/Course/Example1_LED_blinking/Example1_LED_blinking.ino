/*---------------------------------------------------------------
 * LED blinking lesson
 * Drive the board LED with a fixed on/off timing pattern.
 *--------------------------------------------------------------*/

// GPIO connected to the controllable LED.
#define D_PIN 38


/*---------------------------------------------------------------
 * Initialize the lesson
 * Prepare serial diagnostics and configure the LED output.
 *--------------------------------------------------------------*/

/**
 * @brief Prepare the hardware used by the blinking example.
 *
 * The pin must be configured as an output before the program can
 * apply HIGH and LOW voltage levels to the LED circuit.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);
  pinMode(D_PIN, OUTPUT);
}


/*---------------------------------------------------------------
 * Generate the blinking pattern
 * Keep the LED on and off for equal half-second intervals.
 *--------------------------------------------------------------*/

/**
 * @brief Repeat one complete LED blinking cycle.
 *
 * The two delays make the HIGH and LOW phases visible and produce
 * a one-second period with a 50 percent duty cycle.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop() {
  digitalWrite(D_PIN, HIGH);
  delay(500);
  digitalWrite(D_PIN, LOW);
  delay(500);
}
