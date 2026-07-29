#ifndef TOUCH_H
#define TOUCH_H

/*---------------------------------------------------------------
 * GT911 touch interface
 * Control the PCA9557 reset lines and read touch points over I2C.
 *--------------------------------------------------------------*/

#include <Wire.h>

constexpr int TOUCH_SCL = 20;
constexpr int TOUCH_SDA = 19;
constexpr int TOUCH_INT = -1;
constexpr int TOUCH_RST = -1;
constexpr int TOUCH_MAP_X1 = 800;
constexpr int TOUCH_MAP_X2 = 0;
constexpr int TOUCH_MAP_Y1 = 480;
constexpr int TOUCH_MAP_Y2 = 0;
constexpr uint8_t PCA9557_ADDRESS = 0x18;
constexpr uint8_t PCA9557_OUTPUT = 0x01;
constexpr uint8_t PCA9557_CONFIG = 0x03;
constexpr uint8_t GT911_ADDRESS = 0x5D;
constexpr uint16_t GT911_POINT_INFO = 0x814E;
constexpr uint16_t GT911_FIRST_POINT = 0x814F;

// Latest touch position in display coordinates.
int touch_last_x = 0;
int touch_last_y = 0;

// Cached contact state used to rate-limit GT911 polling.
bool touch_is_pressed = false;

/**
 * @brief Write one register in the PCA9557 I2C expander.
 * @param reg Register address.
 * @param value Value to write.
 * @return Nothing.
 * @note Called by touch_init() and the reset helpers.
 */
static void pca9557_write(uint8_t reg, uint8_t value)
{
    Wire1.beginTransmission(PCA9557_ADDRESS);
    Wire1.write(reg);
    Wire1.write(value);
    Wire1.endTransmission();
}

/**
 * @brief Read one register from the PCA9557 expander.
 * @param reg Register address.
 * @return Register value returned by the expander.
 * @note Used before changing an individual expander bit.
 */
static uint8_t pca9557_read(uint8_t reg)
{
    Wire1.beginTransmission(PCA9557_ADDRESS);
    Wire1.write(reg);
    Wire1.endTransmission();
    Wire1.requestFrom(PCA9557_ADDRESS, static_cast<uint8_t>(1));
    return Wire1.read();
}

/**
 * @brief Change one PCA9557 pin between input and output mode.
 * @param pin Expander bit index.
 * @param input true for input, false for output.
 * @return Nothing.
 * @note Preserves the other seven configuration bits.
 */
static void pca9557_set_mode(uint8_t pin, bool input)
{
    uint8_t config = pca9557_read(PCA9557_CONFIG);
    config = input ? config | (1U << pin) : config & ~(1U << pin);
    pca9557_write(PCA9557_CONFIG, config);
}

/**
 * @brief Drive one PCA9557 output bit without disturbing other bits.
 * @param pin Expander bit index.
 * @param high true for logic high, false for logic low.
 * @return Nothing.
 * @note Used by the LCD and touch-controller reset sequence.
 */
static void pca9557_set_state(uint8_t pin, bool high)
{
    uint8_t output = pca9557_read(PCA9557_OUTPUT);
    output = high ? output | (1U << pin) : output & ~(1U << pin);
    pca9557_write(PCA9557_OUTPUT, output);
}

/**
 * @brief Release the display and touch controller from reset.
 * @param None.
 * @return Nothing.
 * @note Called once before the first GT911 transaction.
 */
void touch_init()
{
    pca9557_write(PCA9557_CONFIG, 0xFF);
    pca9557_set_mode(0, false);
    pca9557_set_mode(1, false);
    pca9557_set_state(0, false);
    pca9557_set_state(1, false);
    delay(20);
    pca9557_set_state(0, true);
    delay(100);
    pca9557_set_mode(1, true);
}

/**
 * @brief Read a register block from the GT911 controller.
 * @param reg 16-bit GT911 register address.
 * @param data Destination buffer.
 * @param size Number of bytes to read.
 * @return true when the complete transaction succeeds.
 * @return false when the controller does not acknowledge the request.
 * @note Called by touch_touched() for status and coordinate data.
 */
static bool gt911_read(uint16_t reg, uint8_t *data, size_t size)
{
    Wire1.beginTransmission(GT911_ADDRESS);
    Wire1.write(static_cast<uint8_t>(reg >> 8));
    Wire1.write(static_cast<uint8_t>(reg));
    if(Wire1.endTransmission(false) != 0) return false;
    if(Wire1.requestFrom(GT911_ADDRESS, size) != size) return false;

    for(size_t index = 0; index < size; ++index) data[index] = Wire1.read();
    return true;
}

/**
 * @brief Clear the GT911 status byte after a sample is consumed.
 * @param None.
 * @return Nothing.
 * @note Called at the end of each successful polling cycle.
 */
static void gt911_clear_status()
{
    Wire1.beginTransmission(GT911_ADDRESS);
    Wire1.write(static_cast<uint8_t>(GT911_POINT_INFO >> 8));
    Wire1.write(static_cast<uint8_t>(GT911_POINT_INFO));
    Wire1.write(0);
    Wire1.endTransmission();
}

/**
 * @brief Poll GT911 and publish the first touch point.
 * @param None.
 * @return true when at least one valid contact is active.
 * @return false when no contact is reported or the read fails.
 * @note Called by the LVGL input callback.
 */
bool touch_touched()
{
    static uint32_t last_sample = 0;
    const uint32_t now = millis();
    if(now - last_sample < 30) return touch_is_pressed;
    last_sample = now;

    uint8_t status = 0;
    if(!gt911_read(GT911_POINT_INFO, &status, 1)) return touch_is_pressed;
    if((status & 0x80) == 0) return touch_is_pressed;

    const uint8_t point_count = status & 0x0F;
    touch_is_pressed = point_count > 0 && point_count <= 5;
    if(touch_is_pressed) {
        uint8_t point[7];
        if(gt911_read(GT911_FIRST_POINT, point, sizeof(point))) {
            const uint16_t raw_x = point[1] | (static_cast<uint16_t>(point[2]) << 8);
            const uint16_t raw_y = point[3] | (static_cast<uint16_t>(point[4]) << 8);
            touch_last_x = constrain(raw_x, 0, lcd.width() - 1);
            touch_last_y = constrain(raw_y, 0, lcd.height() - 1);
        }
    }

    gt911_clear_status();
    return touch_is_pressed;
}

#endif
