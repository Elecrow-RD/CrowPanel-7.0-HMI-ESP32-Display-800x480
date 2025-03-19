#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>
#include <Crowbits_DHT20.h>

#define WIFI_SSID "yanfa_software"
#define WIFI_PASSWORD "yanfa-123456"

// Raspberry Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress(192, 168, 50, 233)

// For a cloud MQTT broker, type the domain name
//#define MQTT_HOST "example.com"
#define MQTT_PORT 1885

// Temperature MQTT Topics
#define MQTT_PUB_LED_S  "esp32/led/state"
#define MQTT_PUB_LED_C "esp32/led/command"
#define MQTT_PUB_TEMP "esp32/temperature"
#define MQTT_PUB_HUM  "esp32/humidity"
#define mqtt_username  "elecrow"
#define mqtt_password  "elecrow2014"

// Initialize DHT20 sensor
Crowbits_DHT20 dht20;
int LED = 38;
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

#define USE_UI    //if you want to use the ui export from Squareline, please do not annotate this line.

#if defined USE_UI
#include <lvgl.h>
#include "ui.h"
#endif

#include <Arduino_GFX_Library.h>
#define TFT_BL 2
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
class LGFX : public lgfx::LGFX_Device
{
  public:
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Panel_RGB   _panel_instance;
    LGFX(void)
    {
      {
        auto cfg = _bus_instance.config();
        cfg.panel = &_panel_instance;

        cfg.pin_d0  = GPIO_NUM_15; // B0
        cfg.pin_d1  = GPIO_NUM_7;  // B1
        cfg.pin_d2  = GPIO_NUM_6;  // B2
        cfg.pin_d3  = GPIO_NUM_5;  // B3
        cfg.pin_d4  = GPIO_NUM_4;  // B4

        cfg.pin_d5  = GPIO_NUM_9;  // G0
        cfg.pin_d6  = GPIO_NUM_46; // G1
        cfg.pin_d7  = GPIO_NUM_3;  // G2
        cfg.pin_d8  = GPIO_NUM_8;  // G3
        cfg.pin_d9  = GPIO_NUM_16; // G4
        cfg.pin_d10 = GPIO_NUM_1;  // G5

        cfg.pin_d11 = GPIO_NUM_14; // R0
        cfg.pin_d12 = GPIO_NUM_21; // R1
        cfg.pin_d13 = GPIO_NUM_47; // R2
        cfg.pin_d14 = GPIO_NUM_48; // R3
        cfg.pin_d15 = GPIO_NUM_45; // R4

        cfg.pin_henable = GPIO_NUM_41;
        cfg.pin_vsync   = GPIO_NUM_40;
        cfg.pin_hsync   = GPIO_NUM_39;
        cfg.pin_pclk    = GPIO_NUM_0;
        cfg.freq_write  = 15000000;

        cfg.hsync_polarity    = 0;
        cfg.hsync_front_porch = 40;
        cfg.hsync_pulse_width = 48;
        cfg.hsync_back_porch  = 40;

        cfg.vsync_polarity    = 0;
        cfg.vsync_front_porch = 1;
        cfg.vsync_pulse_width = 31;
        cfg.vsync_back_porch  = 13;

        cfg.pclk_active_neg   = 1;
        cfg.de_idle_high      = 0;
        cfg.pclk_idle_high    = 0;

        _bus_instance.config(cfg);
      }
      {
        auto cfg = _panel_instance.config();
        cfg.memory_width  = 800;
        cfg.memory_height = 480;
        cfg.panel_width  = 800;
        cfg.panel_height = 480;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        _panel_instance.config(cfg);
      }
      _panel_instance.setBus(&_bus_instance);
      setPanel(&_panel_instance);
    }
};


LGFX lcd;

/*******************************************************************************
   Screen Driver Configuration  end
*******************************************************************************/


/*******************************************************************************
   Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

#ifdef USE_UI
/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[800 * 480 / 10];   //notice here!!!  5,7inch: lv_color_t disp_draw_buf[800*480/10]            4.3inch: lv_color_t disp_draw_buf[480*272/10]
//static lv_color_t disp_draw_buf;
static lv_disp_drv_t disp_drv;

int led_flag = 0;
int led_flag_Lock = 0;
// Variables to hold sensor readings
int  temp;
int  hum;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval_time = 10000;        // Interval at which to publish sensor readings

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
#endif

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print( "Data x :" );
      Serial.println( touch_last_x );

      Serial.print( "Data y :" );
      Serial.println( touch_last_y );
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
#endif

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe(MQTT_PUB_LED_C, 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  Serial.println("Publish received.");
  for (int i = 0; i < len; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println("");
  if (strncmp(payload, "ON", 2) == 0) {
    digitalWrite(LED, HIGH);
    mqttClient.publish(MQTT_PUB_LED_S, 0, true, "ON");
    lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
    lv_label_set_text(ui_Label6, "ON");
  }
  if (strncmp(payload, "OFF", 3) == 0) {
    digitalWrite(LED, LOW);
    mqttClient.publish(MQTT_PUB_LED_S, 0, true, "OFF");
    lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
    lv_label_set_text(ui_Label6, "OFF");
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup()
{
  //IO Port Pins
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
  Serial.begin(9600);
  Wire.begin(19, 20);
  Serial.println();
  dht20.begin();
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.onEvent(WiFiEvent);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  mqttClient.setCredentials(mqtt_username, mqtt_password);
  connectToWifi();

  // Init Display
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  delay(200);

#ifdef USE_UI
  lv_init();

  delay(100);
  touch_init();

  screenWidth = lcd.width();
  screenHeight = lcd.height();

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 10);
  //  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, 480 * 272 / 10);
  /* Initialize the display */
  lv_disp_drv_init(&disp_drv);
  /* Change the following line to your display resolution */
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Initialize the (dummy) input device driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
#endif

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

#ifdef USE_UI
  ui_init();//ui from Squareline or GUI Guider
#else
  lcd.fillScreen(TFT_RED);
  delay(800);
  lcd.fillScreen(TFT_BLUE);
  delay(800);
  lcd.fillScreen(TFT_YELLOW);
  delay(800);
  lcd.fillScreen(TFT_GREEN);
  delay(800);
#endif
  Serial.println( "Setup done" );
}

char buffer_t[10];
char buffer_h[10];


void loop()
{
#ifdef USE_UI
  lv_timer_handler();
  delay(5);
#endif
  if (led_flag_Lock == 1)
  {
    if (led_flag == 1)
    {
      led_flag_Lock = 0;
      digitalWrite(LED, HIGH);
      mqttClient.publish(MQTT_PUB_LED_S, 0, true, "ON");
    }
    else
    {
      led_flag_Lock = 0;
      digitalWrite(LED, LOW);
      mqttClient.publish(MQTT_PUB_LED_S, 0, true, "OFF");
    }
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval_time)
  {
    previousMillis = currentMillis;
    // New DHT sensor readings
    hum = (int)dht20.getHumidity();
    // Read temperature as Celsius (the default)
    temp = (int)dht20.getTemperature();
    Serial.print("hum:");
    Serial.println( hum);
    Serial.print("temp:");
    Serial.println( temp);
    itoa(hum, buffer_h, 10);
    itoa(temp, buffer_t, 10);
    lv_label_set_text(ui_Label2, buffer_t);
    lv_label_set_text(ui_Label3, buffer_h);
    if (isnan(temp) || isnan(hum))
    {
      Serial.println(F("Failed to read from DHT20 sensor!"));
      return;
    }
    // Publish an MQTT message on topic esp32/dht20/temperature
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_TEMP, 0, true, String(temp).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_TEMP, packetIdPub1);
    Serial.printf("Message: %.2f \n", temp);
    // Publish an MQTT message on topic esp32/dht20/humidity
    uint16_t packetIdPub2 = mqttClient.publish(MQTT_PUB_HUM, 0, true, String(hum).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_HUM, packetIdPub2);
    Serial.printf("Message: %.2f \n", hum);
  }
}
