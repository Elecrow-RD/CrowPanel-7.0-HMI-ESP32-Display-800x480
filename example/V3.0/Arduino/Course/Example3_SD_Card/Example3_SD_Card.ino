/*---------------------------------------------------------------
 * SD card lesson
 * Mount a card through SPI and display its directory contents.
 *--------------------------------------------------------------*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>


/*---------------------------------------------------------------
 * SD card hardware connections
 * Assign the SPI signals used by the 7.0-inch display board.
 *--------------------------------------------------------------*/

#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12
#define SD_CS   10


/*---------------------------------------------------------------
 * Initialize the card
 * Start SPI, mount the file system, and report the result.
 *--------------------------------------------------------------*/

/**
 * @brief Prepare serial diagnostics and test the SD card.
 *
 * The short delay gives the SPI bus and card power time to settle
 * before the first mount attempt.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  delay(100);

  if (SD_init() == 1) {
    Serial.println("Card Mount Failed");
  }
  else
    Serial.println("initialize SD Card successfully");
}

/**
 * @brief Keep the completed card demonstration idle.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly after setup(); no repeated work is required.
 */
void loop() {
}


/*---------------------------------------------------------------
 * Validate and inspect the SD card
 * Confirm that media is present before reading its capacity and files.
 *--------------------------------------------------------------*/

/**
 * @brief Mount the SD card and print basic information.
 *
 * Directory traversal begins only after both the mount operation and
 * media-type check succeed, preventing invalid file-system access.
 *
 * @param None.
 * @return 0 when initialization succeeds.
 * @return 1 when the card cannot be mounted or no card is detected.
 * @note Called once from setup() after the SPI bus is ready.
 */
int SD_init() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return 1;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No TF card attached");
    return 1;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("TF Card Size: %lluMB\n", cardSize);
  listDir(SD, "/", 2);
  return 0;
}


/*---------------------------------------------------------------
 * Traverse the directory tree
 * Print files and recursively visit folders to a controlled depth.
 *--------------------------------------------------------------*/

/**
 * @brief List one directory and optionally visit its subdirectories.
 *
 * The levels value is reduced at each recursive call. This bounds the
 * traversal depth and prevents the demonstration from descending through
 * an unexpectedly large directory hierarchy.
 *
 * @param fs File-system object that owns the requested directory.
 * @param dirname Path of the directory to open.
 * @param levels Remaining number of subdirectory levels to visit.
 * @return Nothing.
 * @note Called by SD_init() for the root and recursively for folders.
 */
void listDir(fs::FS & fs, const char *dirname, uint8_t levels) {
  File root = fs.open(dirname);
  if (!root) {
    return;
  }

  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());

      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else {
      Serial.print("FILE: ");
      Serial.print(file.name());
      Serial.print("SIZE: ");
      Serial.println(file.size());
    }

    file = root.openNextFile();
  }
}
