# CrowPanel ESP32 7.0-inch V3.0 Product Hardware Driver Guide

| Item | Details |
|---|---|
| Document version | V1.0 |
| Date | 2026-07-29 |
| Author | OpenAI Codex |
| Applicable hardware | CrowPanel ESP32 Display 7.0-inch V3.0, schematic dated 2024-04-20 |
| Main controller | ESP32-S3-WROOM-1-N4R8 |
| Reference software | Working examples and their bundled libraries under `Arduino/Course` in this repository |
| Determination criteria | Working code takes precedence; schematics are used to confirm electrical connections; items shown only in the schematics without examples are marked “hardware confirmed/software not covered” |

## 1. Document Purpose and Evidence Scope

This document is intended for hardware maintenance, Arduino/ESP-IDF driver porting, and onboarding handoffs. Cross-validation is based on the following:

1. Primary hardware evidence: `V3.0/CrowPanel ESP32 Display-7.0-inch-V3.0-20240420.sch`, the PDF of the same name, and the BRD file.
2. Primary software evidence: `Arduino/Course/Example1` through `Example7`, and `LVGL_Arduino7.0`.
3. Secondary software evidence: the `PCA9557`, `TAMC_GT911`, `Crowbits_DHT20`, `LovyanGFX`, and `lvgl` libraries actually referenced by the course code. Other generic examples within these libraries are not considered hardware evidence for this board.
4. The repository does not provide test logs, production test reports, or a BOM. “Verified” means the repository explicitly provides the code as a board-level course or complete-device example. Hardware regression testing is still required after porting to another Arduino-ESP32 version.

### 1.1 Status Definitions

| Status | Meaning |
|---|---|
| A Verified | Evidence exists in both the schematic and board-level examples, and the parameters are consistent or have been resolved according to the code |
| B Hardware confirmed | Clearly present in the schematic, but the repository contains no direct functional example |
| C External example | The code is usable, but the device is not onboard and is connected through an expansion interface |
| D Risk item | The code, comments, or schematic are inconsistent, or the example itself requires correction |

## 2. Peripheral Overview

| Category | Device/Function | Interface or Key GPIO | Software Layer | Status |
|---|---|---|---|---|
| MCU | ESP32-S3-WROOM-1-N4R8, 4 MB Flash/8 MB PSRAM | Core of the entire board | Arduino-ESP32 + ESP-IDF drivers | A |
| Display | 7.0-inch 800×480 RGB565 LCD | RGB16 + PCLK/DE/HSYNC/VSYNC; BL=GPIO2 | LovyanGFX 1.2.21, LVGL 9.1.0 | A |
| Touch | GT911 capacitive touch | I2C SDA=19, SCL=20; software polling | Wire, TAMC_GT911 1.0.2 | A/D |
| I/O expansion | PCA9557 | I2C 0x18; IO0/IO1 board-level control | Wire, PCA9557 1.0.0 | A/D |
| Storage | MicroSD/TF | SPI CS=10, MOSI=11, SCK=12, MISO=13 | SPI, SD, FS | A |
| Audio | NS4168 I2S digital amplifier + speaker connector | DOUT=17, BCLK=42, LRCLK=18 | ESP-IDF legacy I2S | A |
| Wireless | 2.4 GHz Wi-Fi, BLE (integrated into ESP32-S3) | RF handled by the module; no external GPIO | `WiFi.h`, ESP32 BLE | A/D |
| USB/UART | USB-C + CH340C USB-to-serial converter | UART0 TXD0/RXD0, DTR/RTS automatic download | ROM bootloader, Arduino Serial | A |
| Debug UART | 4-pin UART interface J10 | RXD0, TXD0, 3V3, GND | `Serial`, examples use 115200 | B |
| General-purpose interface | J4 2×4 GPIO/I2C interface | GPIO38, SDA19, SCL20, 3V3/GND | GPIO, Wire | A |
| Crowtail interfaces | J6 I2C, J7 GPIO | J6: 20/19; J7: GPIO38 | Wire, GPIO | A |
| External sensor | DHT20 temperature and humidity | J6 I2C, 0x38 | Crowbits_DHT20 | C |
| External positioning | GPS module | UART1 RX=44, TX=43, 9600 8N1 | HardwareSerial + NMEA parsing | C/D |
| Indicators/controls | Power LED, BOOT, RESET, GPIO38 output | BOOT=GPIO0; RESET=EN; D=GPIO38 | GPIO/hardware reset | A/B |
| Power | USB 5 V, battery, charging, power path | 4054A, PMOS, RY3420, MT9201, LP3318 | Autonomous hardware; GPIO2/PCA9557 participate in display control | B |
| LCD bias | 3.3 V, VCOM≈3.8 V, AVDD≈10 V, VGH≈18 V, VGL≈-8.2 V | LCD-POWER-C, LCD-RESET | PCA9557 + hardware power supply | A/B |

## 3. Global GPIO and Multiplexing Matrix

> The ESP32-S3 GPIO Matrix allows flexible mapping of most digital peripherals. “Multiplexing” in the table below refers to the actual assignments in this product and does not indicate fixed chip pins.

| GPIO | Product Function | Direction/Electrical Characteristics | Conflicts and Restrictions |
|---:|---|---|---|
| 0 | LCD PCLK; also connected to the BOOT button/download circuit | High-speed output; strapping input during startup | Must not be externally held at a conflicting level during startup; outputs 24 MHz after display startup |
| 1,3,8,9,16 | RGB G7/G4/G5/G2/G6 | High-speed push-pull output | Cannot be reused while the display is enabled |
| 2 | LCD backlight enable | Push-pull/PWM output, active high | Dedicated to the backlight |
| 4,5,6,7,15 | RGB B7/B6/B5/B4/B3 | High-speed push-pull output | Cannot be reused while the display is enabled |
| 10 | TF CS | Push-pull output, idle high | SPI chip select |
| 11 | TF MOSI | SPI push-pull output | Schematic-compatible with resistive-touch TP_DIN, but this revision uses GT911 |
| 12 | TF SCK | SPI clock output | Schematic-compatible with TP_CLK |
| 13 | TF MISO | SPI input | Schematic-compatible with TP_OUT |
| 14,21,45,47,48 | RGB R3/R4/R7/R5/R6 | High-speed push-pull output | Cannot be reused while the display is enabled; GPIO45 is a strapping pin and must not be externally held at a conflicting level |
| 17 | I2S SDATA | Push-pull output | Onboard amplifier audio data |
| 18 | I2S LRCLK/WS | Push-pull output | Onboard amplifier frame clock |
| 19 | I2C SDA | Open-drain bidirectional, onboard 4.7 kΩ pull-up to 3.3 V | Shared by GT911, PCA9557, J4, and J6 |
| 20 | I2C SCL | Open-drain output, onboard 4.7 kΩ pull-up to 3.3 V | Shared by GT911, PCA9557, J4, and J6 |
| 38 | General-purpose D output/J4/J7 | Push-pull output (in examples) | An obsolete comment in the touch header once specified RST=38, but the actual value is `-1`; do not mistakenly use it as touch reset |
| 39 | LCD HSYNC | High-speed push-pull output | Dedicated to the display |
| 40 | LCD VSYNC | High-speed push-pull output | Dedicated to the display |
| 41 | LCD DE | High-speed push-pull output | Dedicated to the display |
| 42 | I2S BCLK | Push-pull output | Onboard amplifier bit clock |
| 43 | External GPS TX (MCU→GPS) | UART1 push-pull output | Not connected to an onboard device in the schematic; intended for expansion |
| 44 | External GPS RX (GPS→MCU) | UART1 input | Not connected to an onboard device in the schematic; intended for expansion |
| 46 | RGB G3 | High-speed push-pull output | Input capability is restricted by the chip; used only as a display output in this design |

The RGB data array order is B3, B4, B5, B6, B7, G2, G3, G4, G5, G6, G7, R3, R4, R5, R6, R7:

```cpp
const int8_t dataPins[16] = {15, 7, 6, 5, 4, 9, 46, 3,
                             8, 16, 1, 14, 21, 47, 48, 45};
```

## 4. Detailed Driver Guide

### 4.1 ESP32-S3 Main Controller, Flash, and PSRAM

- Model: ESP32-S3-WROOM-1-N4R8. The designation corresponds to 4 MB Quad Flash and 8 MB Octal PSRAM.
- Software: The Arduino framework is built on the ESP-IDF HAL/drivers; the course code also uses the ESP-IDF `driver/i2s.h` directly.
- PSRAM: RGB double buffering has a hard dependency on PSRAM. The code uses two LovyanGFX 800×480 RGB565 buffers. Each frame is approximately 768,000 B, and the double buffer requires approximately 1.46 MiB. OPI PSRAM must be selected under Arduino Tools.
- No bare-metal register operations are used. When porting to ESP-IDF, replace the Arduino `Wire/SPI/SD/Serial` wrappers individually while preserving the pins and timing specified in this document.
- It is recommended to check `psramFound()`, the capacity, and that both framebuffers are non-null immediately at startup. Do not enable the backlight if these checks fail.

### 4.2 RGB LCD and Backlight

#### Pins and Electrical Connections

The LCD connects through the 50-pin FPC connector J2 using 16-bit RGB565. See Section 3 for data and synchronization pins. Backlight GPIO2 connects to the EN pin of the MT9201, which drives LEDA/LEDK through a constant-current boost circuit; a high level turns on the backlight.

#### Verified Configuration

| Parameter | Value |
|---|---:|
| Resolution | 800×480 |
| Pixel format | RGB565, 16 bit |
| PCLK | 24 MHz |
| PCLK sampling edge | `pclk_active_neg = 1`, active on falling edge |
| HSYNC polarity | Active low (0) |
| H front/pulse/back | 40 / 48 / 40 clocks |
| VSYNC polarity | Active low (0) |
| V front/pulse/back | 1 / 31 / 13 lines |
| DE idle state | Low |
| PCLK idle state | Low |
| Buffering strategy | Two full-screen PSRAM buffers, switched at VSYNC |

Key initialization sequence:

```cpp
Wire.begin(19, 20);
Out.reset(); Out.setMode(IO_OUTPUT);
Out.setState(IO0, IO_LOW); Out.setState(IO1, IO_LOW);
delay(20); Out.setState(IO0, IO_HIGH); delay(100);
Out.setMode(IO1, IO_INPUT);
lcd.begin();
digitalWrite(2, HIGH);
```

Dependencies: LovyanGFX 1.2.21 `Bus_RGB`/`Panel_RGB`, and LVGL 9.1.0. LVGL is configured with `LV_COLOR_FORMAT_RGB565` and `LV_DISPLAY_RENDER_MODE_FULL`, and the flush callback calls `presentFrameBuffer()`.

Note: Enable the backlight last to prevent a white or flashing screen before the bias voltages, reset, and RGB clocks have stabilized. For dimming, LEDC PWM can be used on GPIO2. The GPS example retains an old `300 Hz/8 bit/255` branch, but the current code actually uses the digital-high branch.

### 4.3 GT911 Capacitive Touch

#### Interface

| Item | Configuration |
|---|---|
| Bus | I2C, shared Wire bus |
| SDA/SCL | GPIO19 / GPIO20, open-drain, 4.7 kΩ pull-up to 3.3 V |
| Address | Code defaults to 0x5D; the library also supports 0x14 |
| INT/RST | Both are set to `-1` in the code and are not directly controlled by MCU GPIOs |
| Read method | Polling, with no interrupt registered |
| Coordinates | 800×480, `ROTATION_NORMAL`; both X/Y mappings are reversed, 800→0 and 480→0 |
| Touch points | The GT911 library reads up to five points; the course UI uses the first point |

The code calls `Wire.begin(19,20)`, `ts.begin()`, and `ts.setRotation(ROTATION_NORMAL)`. It then periodically calls `ts.read()`, reads the status at `0x814E` and the coordinate registers beginning at `0x814F`, and writes 0 to `0x814E` after reading to clear the status.

Difference note: The schematic provides TP-RESET and INTE, connected through PCA9557 IO0/IO1 to two optional touch FPC connectors, while the course code sets the GT911 INT/RST pins to `-1`. Follow the working code: INT does not participate in the driver, and touch input is polled. TP-RESET is driven by PCA9557 IO0 using a board-level reset sequence of low for 20 ms, followed by high and a 100 ms wait. The trailing header comment `RST -1//38` is a remnant from an older board/design; do not use GPIO38 as the GT911 RST pin on this revision.

### 4.4 PCA9557 I2C GPIO Expander

| Item | Configuration |
|---|---|
| Address | **0x18** |
| SDA/SCL | GPIO19/GPIO20 |
| A0/A1/A2 | All connected to ground in the schematic |
| IO0 | TP-RESET, output |
| IO1 | INTE; initially driven low as an output, then changed to an input after reset |
| IO2 | LCD-RESET, connected to TFT_RESET through a 0 Ω/optional component |
| IO3 | LCD-POWER-C, controls the LCD power-switching chain |
| IO4/IO6 | Locally unconnected/reserved in the schematic |
| IO5/IO7 | No active loads routed out in the schematic |

Registers: Input=0x00, Output=0x01, Polarity=0x02, Configuration=0x03. The outputs are standard CMOS push-pull; the I2C lines are open-drain. `reset()` configures all ports as inputs, sets the output latches high, and disables polarity inversion, after which the board-level code configures IO0/IO1.

The verified code actively configures only IO0 and IO1. Although IO2/IO3 have schematic connections, they remain inputs after `reset()`. The current display power supply/reset behavior primarily relies on 0 Ω/NC assembly options and the default hardware path. If IO2/IO3 are later used to implement LCD power sequencing, the actual component population must first be verified. The driver may then be added only after cold-start, warm-reset, and shutdown tests; assumed signal levels must not be copied directly.

Difference note: The generic description at the top of the library file still states that the PCA9557D has a fixed address of 0x41, but the executable project constant `DEV_ADDR` has been changed to 0x18, and the schematic explicitly specifies 0x18. The driver must use 0x18; do not restore 0x41 based on the outdated comment when porting.

### 4.5 MicroSD/TF Card

| Signal | GPIO | Direction |
|---|---:|---|
| CS | 10 | Output, idle high |
| MOSI/DI | 11 | Output |
| SCK | 12 | Output |
| MISO/DO | 13 | Input |

Initialization: `SPI.begin(12, 13, 11)`, wait 100 ms, then call `SD.begin(10)`. The Arduino `SD`/`FS` layer is used, with a root-directory recursion depth of 2. The example does not explicitly set the SPI frequency and uses the Arduino SD library’s default mounting-frequency/fallback-speed detection strategy. When porting, it is recommended to begin card identification at 400 kHz and then increase to a frequency that is stable for the board and card.

The schematic allows pins 11/12/13 to be reused for resistive-touch DIN/CLK/OUT, but the verified configuration for this product uses the GT911, so there is currently no dynamic bus conflict. If the board is modified to use an XPT2046, separate CS signals must be assigned to the SD card and touch controller, and SPI access must be serialized.

### 4.6 NS4168 I2S Amplifier and Speaker

| Item | Configuration |
|---|---|
| SDATA | GPIO17, MCU output |
| BCLK | GPIO42, MCU output |
| LRCLK/WS | GPIO18, MCU output |
| Mode | I2S master + TX, standard I2S |
| Sampling | 44.1 kHz, 16-bit, stereo right/left |
| DMA | 8 buffers × 64 frames, LEVEL1 interrupt |
| APLL/MCLK | APLL=false, MCLK not output/fixed value 0 |

The example uses the ESP-IDF legacy I2S API: `i2s_driver_install(I2S_NUM_0)`, `i2s_set_pin()`, and `i2s_set_clk()`, and copies the same PCM samples to the left and right channels. The NS4168 CTRL/VOP settings and peripheral network are configured by the hardware; the code performs no additional I2C codec configuration. The differential amplifier output is routed to the speaker through J5; neither terminal may be connected to ground.

The full-scale setting `AMPLITUDE=32767` may cause clipping, amplifier overcurrent, or speaker overheating. Production code should retain 3–6 dB of digital headroom, and the thermal design must be verified for the speaker impedance and power rating.

### 4.7 GPIO38, BOOT, RESET, and LED

- GPIO38: Configured as a push-pull output in the examples. `Example1` toggles it high/low every 500 ms; the LVGL UI drives it high/low according to the button state. This net also connects to J4 P4 and J7 P1 and is adjacent to INTE-related circuitry through an optional resistor. External loads must use 3.3 V logic and must not back-drive the pin.
- BOOT K1: Pressing the button pulls GPIO0 low. GPIO0 is also used as LCD PCLK; it determines download boot mode only during reset sampling and becomes a 24 MHz output during normal operation. Starting the display while BOOT is held may create a contention condition, so it must not be used as a runtime button.
- RESET K4: Pressing the button pulls EN_RESET low. CH340C DTR/RTS signals provide automatic EN/BOOT control through transistors.
- POWER LED: Powered from 3.3 V through a current-limiting resistor. It is a hardware-only indicator with no GPIO control.

### 4.8 Wi-Fi and BLE

Wi-Fi and BLE both use the 2.4 GHz radio integrated into the ESP32-S3 module and do not occupy any GPIOs listed in this document. The board-level antenna keep-out area must remain clear; metal enclosures, batteries, and ribbon cables must not cover the antenna.

- Wi-Fi example: STA mode using `WiFi.begin(ssid,password)`, with automatic reconnection enabled and a blocking wait for `WL_CONNECTED`. Plaintext test SSIDs/passwords in the repository examples must be removed before handoff. Production code should define a connection timeout and configuration-storage strategy.
- BLE example: Device name `ESP32SPI-BLE`. A service UUID is defined, the target characteristic supports read/write/notify, and the initial value is `ELECROW`.
- BLE risk: The current `createCharacteristic()` call does not pass the defined `CHARACTERISTIC_UUID`; it passes only the properties expression. Depending on the BLE library version, this may fail to compile or create an incorrect UUID. Production code should change it to `createCharacteristic(CHARACTERISTIC_UUID, properties)` and then verify advertising, reads/writes, and notifications.

### 4.9 USB-C, CH340C, UART0, and Downloading

- USB-C J3 provides VBUS and connects USB D+/D− to the CH340C through series 22 Ω resistors. CC1 and CC2 each have a 5.1 kΩ pull-down, making the board a USB Device/UFP.
- The CH340C is powered by 3.3 V, with its UART side connected to ESP32-S3 UART0 RXD0/TXD0. DTR/RTS drive EN_RESET and IO0_BOOT to support automatic Arduino downloading.
- J10 4-pin: P1=UART0_RXD0, P2=UART0_TXD0, P3=3V3, P4=GND. Note that the names are from the MCU perspective: an external USB-UART TX must connect to RXD0, and the external RX must connect to TXD0.
- All course examples use a debug serial rate of 115200 baud. UART0 is connected in parallel to the CH340C and J10; an external adapter must not drive it simultaneously.
- The schematic does not route the ESP32-S3 native USB D+/D− signals to J3. Do not assume that this Type-C port supports native USB-OTG/USB CDC functionality.

### 4.10 Expansion Interfaces, DHT20, and GPS

#### Interface Definitions

| Interface | Pin Definitions |
|---|---|
| J4 2×4 | P1 GND, P2 3V3, P3 NC, P4 GPIO38, P5 GND, P6 3V3, P7 SDA19, P8 SCL20 |
| J6 Crowtail I2C | P1 SCL20, P2 SDA19, P3 3V3, P4 GND |
| J7 Crowtail GPIO | P1 GPIO38, P2 unnamed/unconfirmed in the schematic, P3 3V3, P4 GND |
| P2/P4 touch FPC | 1 TP-RESET, 2 3V3, 3 GND, 4 INTE, 5 SDA19, 6 SCL20, 7/8 GND |

The DHT20 is an external sensor connected through J6/J4, not an onboard component in the schematic. Configuration: I2C address 0x38, sharing SDA19/SCL20. `begin()` waits 100 ms and then reads the status using command 0x71. The measurement command is AC 33 00, with polling performed up to 10 times at approximately 20 ms per attempt (10 ms at the calling layer + 10 ms in `readData`). The LVGL example reads temperature and humidity once per second. The shared-bus addresses GT911=0x5D, PCA9557=0x18, and DHT20=0x38 do not conflict.

The GPS is also an external module. The code uses UART1: MCU RX=GPIO44, TX=GPIO43, `9600, SERIAL_8N1`, and parses checksum-validated NMEA lines. The schematic does not define a GPS connector or the electrical breakout for pins 43/44. Before porting or assembly, verify the actual jumper-wire/expansion-board connections, supply voltage, and module I/O levels. Never connect a 5 V UART directly to the ESP32-S3.

### 4.11 Battery, Charging, and System Power

| Function | Schematic Component/Parameter | Software Relationship |
|---|---|---|
| USB input | Type-C VBUS 5 V | No dependency on software enumeration |
| Single-cell lithium battery charging | 4054A, RPROG=2 kΩ, drawing indicates approximately 500 mA | Autonomous hardware; no ADC/status GPIO driver |
| Battery interface | J1, BAT+ / GND | No evidence of a fuel gauge or battery ADC sampling |
| Power path | PMOS 3401, Schottky diode D5, etc. | Automatically supplies power through the USB/battery paths |
| System 3.3 V | RY3420 buck converter; drawing notes that HM3416H has been replaced with RY3420 | MCU and logic power |
| LCD VCOM | RY3420, drawing calculation approximately 3.8 V | Hardware power supply |
| LCD AVDD | LP3318, approximately 10 V | Related to display power sequencing |
| LCD VGH/VGL | Diode/charge pump, approximately +18 V / −8.
2 V | Related to display power sequencing |
| Backlight constant-current driver | MT9201, `Iout=0.2/R29`, R29=1.8 Ω (theoretically approximately 111 mA) | GPIO2 EN/PWM |

Maintenance note: The RY3420 and HM3416H change notes in the schematic title/comments constitute evidence of component selection changes; procurement and repair personnel must reconfirm against the physical component markings and the latest BOM. The LCD positive and negative bias voltages are significantly higher than 3.3 V. Use an appropriate measurement range when measuring with power applied, and avoid shorting the FPC with the probe.

## 5. Initialization and Shutdown Sequences

### 5.1 Recommended Power-On Sequence

1. Start `Serial(115200)` and check PSRAM.
2. Run `Wire.begin(19,20)`, then scan for/confirm PCA9557 at 0x18 and GT911 at 0x5D (also confirm DHT20 at 0x38 if externally connected).
3. Reset all PCA9557 ports; configure IO0/IO1 as low outputs; wait 20 ms; drive IO0 high; wait 100 ms; configure IO1 as an input.
4. Initialize external I2C sensors.
5. First configure GPIO38 as a low output to prevent unintended operation of external actuators.
6. Run `lcd.begin()` and confirm that both RGB framebuffers are successfully allocated.
7. Initialize LVGL, then initialize the GT911 and input device.
8. Finally, drive GPIO2 high or start PWM to turn on the backlight.
9. Initialize SD, I2S, Wi-Fi/BLE, and GPS as needed; avoid lengthy blocking initialization while LVGL is performing real-time refreshes.

### 5.2 Recommended Shutdown/Sleep Sequence

1. Drive GPIO2 low to turn off the backlight first.
2. Stop LVGL refreshes and the RGB PCLK.
3. Set GPIO38 and other actuator outputs to safe states.
4. Stop communication with I2S, SD, and external modules.
5. If supported by the hardware version, use the PCA9557 to turn off LCD power/pull reset low; this repository does not provide fully tested shutdown code, so the sequence must be validated on actual hardware before being finalized.

## 6. List of Schematic and Code Discrepancies

| ID | Item | Schematic/Comments | Validated Code | Determination and Possible Cause |
|---|---|---|---|---|
| D01 | PCA9557 address | Schematic specifies 0x18; generic library documentation specifies 0x41 | `DEV_ADDR=0x18` | Use 0x18; the library comment was not updated to reflect the board-level modification |
| D02 | GT911 RST | Schematic routes TP-RESET through PCA9557 IO0; an outdated header comment references GPIO38 | RST=-1, reset via PCA9557 IO0 | The GPIO38 comment is a remnant from an older board/compatibility template |
| D03 | GT911 INT | Schematic routes INTE through PCA9557 IO1 | INT=-1, polling via `ts.read()` | The current driver does not use interrupts, reducing wiring dependencies but increasing polling overhead |
| D04 | Backlight PWM | The GPS example retains LEDC 300 Hz/8-bit code | The currently active conditional branch digitally turns on the backlight using LOW→HIGH | A legacy dimming experiment branch was not removed; the product should standardize on one implementation |
| D05 | GPS pins | The schematic does not define a GPIO43/44 GPS interface | UART1 RX44/TX43 | GPS is an external course module whose wiring is not included in the mainboard schematic |
| D06 | DHT20 | The schematic does not include an onboard DHT20 | External connection through J6 I2C, 0x38 | This is an external Crowtail module and must not be included in the onboard BOM |
| D07 | Resistive touch | SPI net names retain TP_CLK/DIN/OUT, and optional P2/P4 touch connectors are present | The actual build selects GT911 I2C | The schematic supports compatible configurations; the code configuration represents the current assembly option |
| D08 | BLE characteristic UUID | Not hardware-related | UUID is defined but not passed when the characteristic is created | Example defect; correct and retest before porting |
| D09 | Power IC | Drawing note states that HM3416H was replaced with RY3420 | No software detection | Possibly a board revision/component substitution; verify against the physical hardware/BOM |

## 7. Risks and Precautions

| Priority | Risk | Control Measure |
|---|---|---|
| High | GPIO0 is used for both BOOT strapping and the 24 MHz LCD PCLK | Do not externally force its level during startup; use BOOT only for flashing, not as a runtime button |
| High | The LCD requires approximately +18 V, +10 V, and −8.2 V bias voltages | Prevent shorts during repair; strictly apply power/reset before data/backlight, and reverse the sequence during shutdown |
| High | RGB double buffering depends on OPI PSRAM | Lock the board type/PSRAM build options; do not turn on the backlight if the startup check fails |
| High | GPIO45 is a strapping pin and is also used for RGB R7 | Do not connect additional external circuits; during production validation, verify reliable cold boot, flashing, and display operation together |
| High | The speaker uses an NS4168 differential output | Neither SPK± terminal may be grounded; derate based on impedance, power, and temperature rise |
| Medium | I2C is shared by the touch controller, expander, DHT20, and external interface | Use 3.3 V only; check the effective total pull-up resistance, cable length, and address conflicts; a bus recovery mechanism is recommended |
| Medium | GPIO38 is routed to multiple interfaces | Allow only one peripheral driving source; configure it as a low output at power-on; verify load current, and do not directly drive relays/high-current LEDs |
| Medium | GPS wiring is not included in the schematic | Add GPIO43/44, power, and ground to the assembly drawing; confirm that the UART level is 3.3 V |
| Medium | Example Wi-Fi credentials are hard-coded in plaintext | Remove them before committing production firmware; use secure provisioning/a controlled configuration area |
| Medium | Incorrect parameters are used when creating the BLE example characteristic | Correct the UUID parameter, then perform build, scanning, connection, read/write, and notify regression testing |
| Medium | The legacy I2S API may change in newer Arduino-ESP32/ESP-IDF releases | Pin the toolchain version, or migrate to the new I2S channel API and perform audio regression testing |
| Low | The schematic includes NC/0 Ω optional components and two touch connection options | Verify the actual component population before repair; do not infer installed components solely from net names |

## 8. Porting Checklist

- [ ] Set the target to ESP32-S3-WROOM-1-N4R8 and enable OPI PSRAM.
- [ ] Preserve the GPIO assignments in Section 3; pay particular attention to GPIO0, GPIO45, GPIO46, and the RGB group.
- [ ] Use SDA19/SCL20 for I2C and confirm responses from 0x18/0x5D/optional 0x38.
- [ ] Reproduce the PCA9557 reset sequence: low for 20 ms, then high, followed by a 100 ms delay.
- [ ] Match the RGB timing, falling-edge sampling, and double-buffering parameters item by item.
- [ ] Turn on the backlight last and turn it off first during shutdown.
- [ ] Verify SD read/write, directory traversal, and hot-plug exception handling.
- [ ] Verify that 44.1 kHz I2S audio has no popping, overheating, or clipping.
- [ ] Validate UART0 flashing/logging and UART1 GPS reception separately.
- [ ] Correct the BLE characteristic UUID and remove plaintext Wi-Fi credentials.
- [ ] Verify the 3.3 V logic level, ground connection, and maximum load for each expansion interface.
- [ ] Pass cold-start, warm-reset, undervoltage, USB/battery switchover, and extended display burn-in tests.

## 9. Key Source Code Index

| Function | File and Key Locations |
|---|---|
| RGB/LVGL/PCA9557/DHT20 | `Arduino/Course/LVGL_Arduino7.0/LVGL_Arduino7.0.ino`: RGB configuration at approximately lines 44–69; initialization at approximately lines 175–248 |
| GT911 board-level configuration | `Arduino/Course/LVGL_Arduino7.0/touch.h`: pins at approximately lines 23–31; initialization at approximately lines 136–147 |
| Integrated LCD/GPS example | `Arduino/Course/Example7_GPS_Module/Example7_GPS_Module.ino`: GPS pins at lines 20–21; full-system initialization at approximately lines 575–644 |
| TF card | `Arduino/Course/Example3_SD_Card/Example3_SD_Card.ino`: pins at lines 17–20; initialization at lines 38–47 and 77–92 |
| I2S audio | `Arduino/Course/Example2_Play_music/Example2_Play_music.ino`: pins at lines 16–18; I2S configuration at lines 89–112 |
| GPIO38 | `Arduino/Course/Example1_LED_blinking/Example1_LED_blinking.ino` |
| Wi-Fi | `Arduino/Course/Example6_WIFI/Example6_WIFI.ino` |
| BLE | `Arduino/Course/Example5_BLE/Example5_BLE.ino`: UUIDs at lines 24–28; service/characteristic creation at lines 82–94 |
| PCA9557 registers/address | `Arduino/libraries/PCA9557/src/PCA9557.h`: address at approximately line 113; registers at approximately lines 126–129 |
| DHT20 protocol | `Arduino/libraries/Crowbits_DHT20/Crowbits_DHT20.cpp` |
| GT911 registers | `Arduino/libraries/gt911-arduino-main/TAMC_GT911.h`, `TAMC_GT911.cpp` |
| Schematic | `V3.0/CrowPanel ESP32 Display-7.0-inch-V3.0-20240420.sch` and the PDF with the same name |

## 10. Version History

| Version | Date | Changes |
|---|---|---|
| V1.0 | 2026-07-29 | Initial release: completed systematic cross-validation of the schematic, board-level course code, and direct dependency libraries |