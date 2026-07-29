/*---------------------------------------------------------------
 * Wi-Fi station lesson
 * Join an access point and report the assigned network address.
 *--------------------------------------------------------------*/

#include <WiFi.h>

// Credentials used by the ESP32 when joining the wireless network.
const char *ssid = "yanfa1";
const char *password = "1223334444yanfa";


/*---------------------------------------------------------------
 * Connect to Wi-Fi
 * Wait for a complete station connection before using network details.
 *--------------------------------------------------------------*/

/**
 * @brief Connect the board to the configured wireless network.
 *
 * Automatic reconnection allows the Wi-Fi stack to recover after a brief
 * loss of coverage without requiring application-level retry logic.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  // Network information is valid only after the station has associated.
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("connecting");
  }

  Serial.println("WiFi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Keep the completed connection demonstration idle.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly after setup(); Wi-Fi maintenance runs internally.
 */
void loop() {
}
