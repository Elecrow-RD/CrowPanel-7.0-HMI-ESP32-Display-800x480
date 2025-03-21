#include <PCA9557.h>
#include <lvgl.h>
#include <Crowbits_DHT20.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>


#include <SPI.h>



#include <Adafruit_GFX.h>


#define TFT_BL 2

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


//UI
#include "ui.h"
int led;
Crowbits_DHT20 dht20;
SPIClass& spi = SPI;
// uint16_t touchCalibration_x0 = 300, touchCalibration_x1 = 3600, touchCalibration_y0 = 300, touchCalibration_y1 = 3600;
// uint8_t  touchCalibration_rotate = 1, touchCalibration_invert_x = 2, touchCalibration_invert_y = 0;
// static int val = 100;
/*******************************************************************************
   Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"
// #include "Audio.h"
// Audio audio;
//String music_url;
//int music_id = 1818615872;


/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
//static lv_color_t *disp_draw_buf;
static lv_color_t disp_draw_buf[800 * 480 / 15];
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{

  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  

  //lcd.fillScreen(TFT_WHITE);
#if (LV_COLOR_16_SWAP != 0)
 lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);//
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
      Serial.print( "Data x " );
      Serial.println( data->point.x );
      Serial.print( "Data y " );
      Serial.println( data->point.y );
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



PCA9557 Out;
void setup()
{
  
  Serial.begin(9600);
  Serial.println("LVGL Widgets Demo");

  //IO口引脚
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);


  delay(200); 
  //触摸屏上电时序
  //  Wire.setSDA(19);
  //  Wire.setSCL(20);
  Wire.begin(19, 20);
  dht20.begin();
  Out.reset();
  Out.setMode(IO_OUTPUT);  //设置为输出模式

  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);
//  EEPROM.begin(1024);
//  if (get_string(1, 0) == e_flag) { //如果第0位字符为“M”，则表示已储存WIFI信息
//    Serial.println("已储存信息");
//    pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
//    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
//    SPI.setFrequency(1000000);
//    SD.begin(SD_CS);
//    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
//    audio.setVolume(15); // 0...21
//    audio.connecttoFS(SD, "/123.mp3");
//    while (1)
//    {
//      unsigned long currentMillis = millis();
//      //检查是否到达时间间隔
//      if (currentMillis - previousBlinkTime >= blinkInterval) {    //如果时间间隔达到了
//        audio.stopSong();
//        break;
//      }
//      else if (currentMillis - previousBlinkTime <= 0)
//      { // 如果millis时间溢出
//        previousBlinkTime = currentMillis;
//      }
//      audio.loop();
//    }
//  }
//  else {
//    Serial.println("未储存有WIFI信息,将储存wifi信息");
//    //保存信息
//    set_string(1, 0, e_flag, 0); //录入"N"字符
//    Serial.println("完成储存");
//    Serial.println("即将重启");
//    ESP.restart();
//  }
//  EEPROM.end();


  
  // Init Display
  lcd.begin();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  delay(200);
  

//    lcd.fillScreen(TFT_RED);
//    delay(1000);
//    lcd.fillScreen(TFT_GREEN);
//    delay(1000);
//    lcd.fillScreen(TFT_BLUE);
//    delay(1000);
//    lcd.fillScreen(TFT_BLACK);
//    delay(1000);
  lv_init();


  // Init touch device
  touch_init();

  screenWidth = lcd.width();
  screenHeight = lcd.height();

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 15); //4

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
#ifdef TFT_BL
 
  //digitalWrite(TFT_BL, HIGH);
  ledcSetup(1, 300, 8);
  ledcAttachPin(TFT_BL, 1);
  ledcWrite(1, 255); /* Screen brightness can be modified by adjusting this parameter. (0-255) */
#endif
 
  #ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); 
  delay(500);
  digitalWrite(TFT_BL, HIGH);
  #endif

  
  ui_init();//开机UI界面
  

  lv_timer_handler();

  

}

void loop()
{

  lv_timer_handler(); /* let the GUI do its work */
  delay( 10 );
   
}
