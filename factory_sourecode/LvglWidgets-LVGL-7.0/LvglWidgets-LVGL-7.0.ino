#include <PCA9557.h>
#include <lvgl.h>
#include <demos/lv_demos.h>
#include <examples/lv_examples.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
//#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>/
#include <SD.h>
#include <FS.h>
//U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C  u8g2(U8G2_R0, /* clock=*/ 20, /* data=*/ 19, /* reset=*/ U8X8_PIN_NONE);     // Adafruit Feather ESP8266/32u4 Boards + FeatherWing OLED
#include <Adafruit_SSD1306.h> 
#include <Adafruit_GFX.h>
#define oled_sda 19 
#define oled_scl 20
#define OLED_RESET     -1 
 
//新建iic对象以及显示屏对象
TwoWire I2Cone = TwoWire(0);
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &I2Cone,OLED_RESET);
 
/*******************************************************************************

 ******************************************************************************/
//#include <Arduino_GFX_Library.h>
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


/*******************************************************************************
   End of Arduino_GFX setting
 ******************************************************************************/
#include <EEPROM.h>
String e_flag = "B"; //如果想要修改已保存的WIFI信息，改下这个字母就可以
#include <WiFi.h>
#include <Ticker.h>          //Call the ticker. H Library
Ticker ticker1;
#include "WiFiMulti.h"
WiFiMulti wifiMulti;
#include "BLEDevice.h"              //BLE驱动库
#include "BLEServer.h"              //BLE蓝牙服务器库
#include "BLEUtils.h"               //BLE实用程序库
#include "BLE2902.h"                //特征添加描述符库
#include <BLECharacteristic.h>      //BLE特征函数库
BLEAdvertising* pAdvertising = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
#define bleServerName "Wizee-Series-7.0"  //BLE服务器的名称
#define SERVICE_UUID "6479571c-2e6d-4b34-abe9-c35116712345"  //服务的UUID
#define CHARACTERISTIC_UUID "826f072d-f87c-4ae6-a416-6ffdcaa02d73"
bool connected_state = false;   //创建设备连接标识符

class MyServerCallbacks: public BLEServerCallbacks  //创建连接和断开调用类
{
    void onConnect(BLEServer *pServer)//开始连接函数
    {
      connected_state = true;   //设备正确连接
    }
    void onDisconnect(BLEServer *pServer)//断开连接函数
    {
      connected_state = false;  //设备连接错误
    }

};


char CloseData;
int NO_Test_Flag = 0;
int Test_Flag = 0;
int Close_Flag = 0;

#define SERIAL_BAUD 9600
HardwareSerial cardSerial(1);//声明串口1

const char *ssid = "elecrow888"; //你的网络名称
const char *password = "elecrow2014"; //你的网络密码
char buf[100] = {};
int bufindex = 0;
int wifi_close_flag = 0;
char *info[100] = {};
int wifi_flag = 0;
int i = 0;
int home_flag = 0;
int touch_flag = 0;

//7.0
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10
unsigned long previousBlinkTime;
int blinkInterval = 15000; //LED闪烁时间间隔
#define I2S_DOUT      17
#define I2S_BCLK      42
#define I2S_LRC       18
#define BUTTON_PIN    38

//UI
#include "ui.h"
static int first_flag = 0;
extern int zero_clean;
extern int goto_widget_flag;
extern int bar_flag;
extern lv_obj_t * ui_MENU;
extern lv_obj_t * ui_TOUCH;
extern lv_obj_t * ui_JIAOZHUN;
extern lv_obj_t * ui_Label2;
static lv_obj_t * ui_Label;//TOUCH界面label
static lv_obj_t * ui_Label3;//TOUCH界面label3
static lv_obj_t * ui_Labe2;//Menu界面进度条label
static lv_obj_t * bar;//Menu界面进度条
SPIClass& spi = SPI;
uint16_t touchCalibration_x0 = 300, touchCalibration_x1 = 3600, touchCalibration_y0 = 300, touchCalibration_y1 = 3600;
uint8_t  touchCalibration_rotate = 1, touchCalibration_invert_x = 2, touchCalibration_invert_y = 0;
static int val = 100;
/*******************************************************************************
   Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"
#include "Audio.h"
Audio audio;
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

//读取或保存信息；
void save_or_read_wifi(void) {
  EEPROM.begin(1024);
  if (get_string(1, 0) == e_flag) { //如果第0位字符为“M”，则表示已储存WIFI信息
    Serial.println("已储存信息");
  }
  else {
    Serial.println("未储存有WIFI信息,将储存wifi信息");
    //保存信息
    set_string(1, 0, e_flag, 0); //录入"N"字符
    Serial.println("完成储存");
    Serial.println("即将重启");
    ESP.restart();
  }
  EEPROM.end();
}

//用EEPROM的a位保存字符串的长度，字符串的从EEPROM的b位开始保存，str为要保存的字符串，s为是否保存字符串长度
void set_string(int a, int b, String str, int s)
{
  if (s)EEPROM.write(a, str.length()); //EEPROM第a位，写入str字符串的长度
  //通过一个for循环，把str所有数据，逐个保存在EEPROM
  for (int i = 0; i < str.length(); i++) {
    EEPROM.write(b + i, str[i]);
  }
  EEPROM.commit();  //执行保存EEPROM

}

//获取指定EEPROM位置的字符串，a是字符串长度，b是起始位，从EEPROM的b位开始读取
String get_string(int a, int b) {
  String data = "";
  //通过一个for循环，从EEPROM中逐个取出每一位的值，并连接起来
  for (int i = 0; i < a; i++) {
    data += char(EEPROM.read(b + i));
  }
  return data;
}

void callback1()  //Callback function
{
  if (bar_flag == 6)
  {
    if (val > 1)
    {
      val--;
      lv_bar_set_value(bar, val, LV_ANIM_OFF);
      lv_label_set_text_fmt(ui_Labe2, "%d %%", val);
    }
    else
    {
      lv_obj_clear_flag(ui_touch, LV_OBJ_FLAG_CLICKABLE);
      lv_label_set_text(ui_Labe2, "Loading");
      delay(150);
      val = 100;
      bar_flag = 0; //停止进度条标志
      goto_widget_flag = 1; //进入widget标志

    }
  }
}





//触摸Label控件
void label_xy()
{
  ui_Label = lv_label_create(ui_TOUCH);
  lv_obj_enable_style_refresh(true);
  lv_obj_set_width(ui_Label, LV_SIZE_CONTENT);   /// 1
  lv_obj_set_height(ui_Label, LV_SIZE_CONTENT);    /// 1
  lv_obj_set_x(ui_Label, -55);
  lv_obj_set_y(ui_Label, -40);
  lv_obj_set_align(ui_Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_color(ui_Label, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label3 = lv_label_create(ui_TOUCH);
  lv_obj_enable_style_refresh(true);
  lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);   /// 1
  lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);    /// 1
  lv_obj_set_x(ui_Label3, 85);
  lv_obj_set_y(ui_Label3, -40);
  lv_obj_set_align(ui_Label3, LV_ALIGN_CENTER);
  lv_obj_set_style_text_color(ui_Label3, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_Label3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

}


//进度条控件
void lv_example_bar(void)
{
  //////////////////////////////
  bar = lv_bar_create(ui_MENU);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_width(bar, 480);
  lv_obj_set_height(bar, 25);
  lv_obj_set_x(bar, 0);
  lv_obj_set_y(bar, 175);
  lv_obj_set_align(bar, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_img_src(bar, &ui_img_bar_800_01_png, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_bg_img_src(bar, &ui_img_bar_800_02_png, LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(bar, lv_color_hex(0x2D8812), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
  //////////////////////
  ui_Labe2 = lv_label_create(bar);//创建标签
  lv_obj_set_style_text_color(ui_Labe2, lv_color_hex(0x09BEFB), LV_STATE_DEFAULT);
  lv_label_set_text(ui_Labe2, "0%");
  lv_obj_center(ui_Labe2);
}


void Ce_shi() {
  char serialData = Serial.read();
  int IIC_i=0;
  switch (serialData) {
    case 'R':
      Close_Flag = 1;
      //      WiFi.disconnect();//断开连接
      lcd.fillScreen(TFT_BLACK);
      Serial.println("红屏");
      while (Close_Flag == 1)
      {
        lcd.fillScreen(TFT_RED);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出红屏");
          break;
        }
      }
      break;

    case 'G':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("绿屏");
      while (Close_Flag == 1)
      {
        lcd.fillScreen(TFT_GREEN);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出绿屏");
          break;
        }
      }
      break;

    case 'B':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("蓝屏");
      while (Close_Flag == 1)
      {
        lcd.fillScreen(TFT_BLUE);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出蓝屏");
          break;
        }
      }
      break;
      case 'w':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("白屏");
      while (Close_Flag == 1)
      {
        lcd.fillScreen(TFT_WHITE);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出白屏");
          break;
        }
      }
      break;
      case 'h':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("灰屏");
      while (Close_Flag == 1)
      {
        lcd.fillScreen(TFT_DARKGREY);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出灰屏");
          break;
        }
      }
      break;
    case 'S':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("TF卡初始化");
      SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
      SD_init();
      while (Close_Flag == 1)
      {
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出TF卡初始化");
          break;
        }
      }
      break;

    case 'T':
      Close_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("触摸屏");
      touch_flag = 0;
      delay(100);
      lcd.setCursor(300, 120);
      lcd.printf("Please Touch");
      Serial.println("TOUCH");
      touch_last_x = 200;
      touch_last_y = 200;
      delay(1000);
      while (Close_Flag == 1)
      {
        if (touch_flag < 3)
        {
          if (touch_has_signal())
          {
            if (touch_touched())
            {
              if (touch_last_x > 0 && touch_last_x < 100 && touch_last_y > 0 && touch_last_y < 100 && touch_flag == 0)
              {
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(320, 200);
                lcd.printf("               ");
                lcd.setCursor(320, 250);
                lcd.printf("               ");

                lcd.setCursor(320, 200);
                lcd.printf("x: %i     ", touch_last_x);
                lcd.setCursor(320, 250);
                lcd.printf("y: %i    ", touch_last_y);
                Serial.print( "Data x :" );
                Serial.println( touch_last_x );
                Serial.print( "Data y :" );
                Serial.println( touch_last_y );
                touch_flag = 1;

              }
              if (touch_last_x > 700 && touch_last_x < 800 && touch_last_y > 400 && touch_last_y < 480 && touch_flag == 1)
              {
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(320, 200);
                lcd.printf("               ");
                lcd.setCursor(320, 250);
                lcd.printf("               ");

                lcd.setCursor(320, 200);
                lcd.printf("x: %i     ", touch_last_x);
                lcd.setCursor(320, 250);
                lcd.printf("y: %i    ", touch_last_y);
                Serial.print( "Data x :" );
                Serial.println( touch_last_x );
                Serial.print( "Data y :" );
                Serial.println( touch_last_y );
                touch_flag = 3;
                delay(100);
                Serial.println("OVER");
              }
              if (touch_last_x > 0 && touch_last_x < 4000 && touch_last_y > 0 && touch_last_y < 4000)
              {
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(320, 200);
                lcd.printf("               ");
                lcd.setCursor(320, 250);
                lcd.printf("               ");

                lcd.setCursor(320, 200);
                lcd.printf("x: %i     ", touch_last_x);
                lcd.setCursor(320, 250);
                lcd.printf("y: %i    ", touch_last_y);
                Serial.print( "Data x :" );
                Serial.println( touch_last_x );
                Serial.print( "Data y :" );
                Serial.println( touch_last_y );
              }
              CloseData = Serial.read();
              if (CloseData == 'I')  //Exit
              {
                Close_Flag = 0;
                lcd.fillScreen(TFT_BLACK);
                break;
              }
              delay(80);
            }
          }
        }
        if (touch_flag == 3)
        {

          if (touch_touched())
          {
            lcd.fillScreen(TFT_BLACK);
            if (touch_last_x > 0 && touch_last_x < 4000 && touch_last_y > 0 && touch_last_y < 4000)
            {
              lcd.setCursor(320, 200);
              lcd.printf("               ");
              lcd.setCursor(320, 250);
              lcd.printf("               ");

              lcd.setCursor(320, 200);
              lcd.printf("x: %i     ", touch_last_x);
              lcd.setCursor(320, 250);
              lcd.printf("y: %i    ", touch_last_y);
              Serial.print( "Data x :" );
              Serial.println( touch_last_x );
              Serial.print( "Data y :" );
              Serial.println( touch_last_y );
            }
          }
        }
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出触摸屏");
          break;
        }
      }
      break;

    case 'L':
      Close_Flag = 1;
      Serial.println("IO口测试");
      lcd.fillScreen(TFT_BLACK);
      pinMode(38, OUTPUT);
      digitalWrite(38, LOW);
      lcd.setCursor(260, 230);
      lcd.printf("IO port output testing");
      while (Close_Flag == 1)
      {
        digitalWrite(38, HIGH);//拉高电平开灯
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          digitalWrite(38, LOW);
          lcd.fillScreen(TFT_BLACK);
          Serial.println("退出IO口测试");
          break;
        }
      }
      break;

    case 'i':
        Close_Flag = 1;
        I2Cone.begin(oled_sda, oled_scl); 
        //初始化显示屏
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        //清屏
        display.clearDisplay();
        //设置字体大小
        display.setTextSize(2);
        display.setTextColor(INVERSE); 
        display.setTextColor(WHITE);//开像素点发光
        Serial.println("IIC");
        lcd.fillScreen(TFT_BLACK);
        lcd.setCursor(280, 230);
        lcd.printf("IIC testing");  
    
        while (Close_Flag == 1)
        {

            IIC_i+=10;
            display.clearDisplay();  
            display.setCursor(120-IIC_i,19);
            display.print("Elecrow");
            display.display();  
            delay(400);
          if(IIC_i>120)
            {
              IIC_i=0;  
            }
          CloseData = Serial.read();
          if (CloseData == 'I')  //Exit
          {
            Close_Flag = 0;
            lcd.fillScreen(TFT_BLACK);
            Serial.println("退出IIC");
            break;
          }
        }
        break;

    case 'W':
      Close_Flag = 1;
      Serial.println("Wifi function testing");
      lcd.fillScreen(TFT_BLACK);
      lcd.setCursor(280, 230);
      lcd.printf("WiFi testing");
      wifi_flag = 0;
      Serial.flush();//清空串口缓存
      memset(buf, 0, sizeof(buf));
      bufindex = 0;
      while (Close_Flag == 1)
      {
        if(Serial.available())
        {
          delay(200);
        }
        while (Serial.available() && wifi_flag == 0)
        {
          
          CloseData = Serial.read();
          buf[bufindex] = CloseData;
          bufindex+=1;
          if (CloseData == '\n')
          {
            buf[bufindex] = '\0';
            Serial.printf("buf==");
            Serial.println(buf);
            info[0] = buf;
            info[0] = strtok_r(info[0], "@", &info[1]);
            info[1] = strtok_r(info[1], "@", &info[2]);
            //sscanf(buf, "@%s@%s\n", wifi_account, wifi_password);
            delay(5);
            wifi_flag = 1;
          }
          if (bufindex >= 128)
          {
            bufindex = 0;
          }
        }
        
        if (wifi_flag == 1)
        {
          // 输出 SSID 和密码到串口监视器
          Serial.print("SSID: ");
          Serial.println(info[0]);
          
          Serial.print("Password: ");
          Serial.println(info[1]);
          
          WiFi.disconnect();
          delay(500);
          WiFi.begin(info[0], info[1]);
          delay(500);
          Serial.println("begining");

          while (WiFi.status()  != WL_CONNECTED)
          {
            delay(100);
            CloseData = Serial.read();
            if (CloseData == 'I')  //Exit
            {
              memset(buf, 0, 100);
              bufindex = 0;
              break;
            }
            Serial.println("connecting");
          }
          if(WiFi.status() != WL_CONNECTED)
            {
                Serial.println("WiFi is not connected.");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(280, 230);
                lcd.printf("WiFi is not connected.");
            }else
            {
                Serial.println("WiFi is connected.");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(280, 230);
                lcd.printf("WiFi is connected");
            }

          WiFi.disconnect();
          WiFi.mode(WIFI_OFF);
          wifi_flag = 2;
          memset(buf, 0, 100);
          bufindex = 0;
        
        }
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          wifi_flag = 0;
          Close_Flag = 0;
          memset(buf, 0, 100);
          bufindex = 0;
          WiFi.disconnect();
          lcd.fillScreen(TFT_BLACK);
          Serial.println("Exit Wifi function testing");
          
          break;
        }
      }
      break;


    case 'U':
      Close_Flag = 1;
      Serial.println("打开UART1");
      lcd.fillScreen(TFT_BLACK);
      lcd.setCursor(280, 230);
      lcd.printf("UART1 testing");
      while (Close_Flag == 1)
      {
        //  如果硬件串口有数据
        if (Serial.available())
        {
          //从硬件串口读出一字节，写入软件串口
          Serial.write(Serial.read());
        }
        //        //如果软件串口有数据
        //        if (cardSerial.available())
        //        {
        //          //从软件串口读出一字节，写入硬件串口
        //          Serial.write(cardSerial.read());
        //        }
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          //          digitalWrite(25, LOW);
          lcd.fillScreen(TFT_BLACK);
          Serial.println("关闭UART1");
          break;
        }
      }
      break;

    case 'P':
      Close_Flag = 1;
      Serial.println("打开SPEAK");
      lcd.fillScreen(TFT_BLACK);
      lcd.setCursor(280, 230);
      lcd.printf("SPEAK testing");
      WiFi.disconnect();
      delay(2000);
      while (Close_Flag == 1)
      {
//                audio.loop();
//                EEPROM.begin(1024);
//                set_string(1,0,"L",0); //录入"L"字符
//                Serial.println("完成储存");
//                Serial.println("即将重启");
//                ESP.restart();
//                EEPROM.end();
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          //          audio.stopSong();
          //          pinMode(BUTTON_PIN, OUTPUT);
          //          digitalWrite(BUTTON_PIN, LOW);
          //          WiFi.disconnect();
          delay(50);
          Close_Flag = 0;
          lcd.fillScreen(TFT_BLACK);
          Serial.println("关闭SPEAK");
          break;
        }
      }
      break;

    case 'V':
      Close_Flag = 1;
      Serial.println("Bluetooth function");
      pAdvertising->start();  //Start broadcasting
      pService->start();
      delay(500);
      lcd.fillScreen(TFT_BLACK);
      while (Close_Flag == 1)
      {
        if (connected_state == true) //有设备连接
        {
          lcd.fillScreen(TFT_BLACK);
          lcd.setCursor(260, 230);
          lcd.printf("Bluetooth is connected");
        }
        else
        {
          lcd.fillScreen(TFT_BLACK);
          lcd.setCursor(260, 230);
          lcd.printf("Bluetooth is not connected");
        }
        delay(100);
        CloseData = Serial.read();
        if (CloseData == 'I')  //Exit
        {
          Close_Flag = 0;
          pAdvertising->stop();  //停止广播
          pService->stop();
          delay(50);
          lcd.fillScreen(TFT_BLACK);
          Serial.println("Turn off Bluetooth function");
          break;
        }
      }
      break;

    //Exit软件测试程序
    case 'C':
      WiFi.disconnect();
      NO_Test_Flag = 0;
      Test_Flag = 0;
      //      Wire.end();
      lcd.fillScreen(TFT_BLACK);
      //      lv_init();
      Serial.println("Exit the software test program");
      delay(500);
      break;
  }
}


//SD卡初始化
int SD_init()
{

  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return 1;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No TF card attached");
    return 1;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("TF Card Size: %lluMB\n", cardSize);
  listDir(SD, "/", 2);

  //  listDir(SD, "/", 0);
  //  createDir(SD, "/mydir");
  //  listDir(SD, "/", 0);
  //  removeDir(SD, "/mydir");
  //  listDir(SD, "/", 2);
  //  writeFile(SD, "/hello.txt", "Hello ");
  //  appendFile(SD, "/hello.txt", "World!\n");
  //  readFile(SD, "/hello.txt");
  //  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  //  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  //  Serial.println("SD init over.");

  return 0;
}

//遍历SD卡目录
void listDir(fs::FS & fs, const char *dirname, uint8_t levels)
{
  //  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    //Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  i = 0;
  while (file)
  {
    if (file.isDirectory())
    {
      //      Serial.print("  DIR : ");
      //      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("FILE: ");
      Serial.print(file.name());
      lcd.setCursor(0, 2 * i);
      lcd.printf("FILE:%s", file.name());

      Serial.print("SIZE: ");
      Serial.println(file.size());
      lcd.setCursor(180, 2 * i);
      lcd.printf("SIZE:%d", file.size());
      i += 16;
    }

    file = root.openNextFile();
  }
}

// optional
void audio_info(const char *info) {
  Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info) { //id3 metadata
  Serial.print("id3data     "); Serial.println(info);
}
void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3     "); Serial.println(info);
}
void audio_showstation(const char *info) {
  Serial.print("station     "); Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle "); Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     "); Serial.println(info);
}
void audio_commercial(const char *info) { //duration in sec
  Serial.print("commercial  "); Serial.println(info);
}
void audio_icyurl(const char *info) { //homepage
  Serial.print("Please search again"); Serial.println(info);
}
void audio_lasthost(const char *info) { //stream URL played
  Serial.print("lasthost    "); Serial.println(info);
}



#define Z_THRESHOLD 350 // Touch pressure threshold for validating touches
#define _RAWERR 20 // Deadband error allowed in successive position samples
void begin_touch_read_write(void) {
  digitalWrite(38, HIGH); // Just in case it has been left low
  spi.setFrequency(600000);
  digitalWrite(38, LOW);
}

void end_touch_read_write(void) {
  digitalWrite(38, HIGH); // Just in case it has been left low
  spi.setFrequency(600000);

}

uint16_t getTouchRawZ(void) {

  begin_touch_read_write();

  // Z sample request
  int16_t tz = 0xFFF;
  spi.transfer(0xb0);               // Start new Z1 conversion
  tz += spi.transfer16(0xc0) >> 3;  // Read Z1 and start Z2 conversion
  tz -= spi.transfer16(0x00) >> 3;  // Read Z2

  end_touch_read_write();

  return (uint16_t)tz;
}

uint8_t getTouchRaw(uint16_t *x, uint16_t *y) {
  uint16_t tmp;

  begin_touch_read_write();

  // Start YP sample request for x position, read 4 times and keep last sample
  spi.transfer(0xd0);                    // Start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion

  tmp = spi.transfer(0);                   // Read first 8 bits
  tmp = tmp << 5;
  tmp |= 0x1f & (spi.transfer(0x90) >> 3); // Read last 8 bits and start new XP conversion

  *x = tmp;

  // Start XP sample request for y position, read 4 times and keep last sample
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion

  tmp = spi.transfer(0);                 // Read first 8 bits
  tmp = tmp << 5;
  tmp |= 0x1f & (spi.transfer(0) >> 3);  // Read last 8 bits

  *y = tmp;

  end_touch_read_write();

  return true;
}

uint8_t validTouch(uint16_t *x, uint16_t *y, uint16_t threshold) {
  uint16_t x_tmp, y_tmp, x_tmp2, y_tmp2;

  // Wait until pressure stops increasing to debounce pressure
  uint16_t z1 = 1;
  uint16_t z2 = 0;
  while (z1 > z2)
  {
    z2 = z1;
    z1 = getTouchRawZ();
    delay(1);
    Serial.print("z1:");
    Serial.println(z1);
  }


  if (z1 <= threshold) return false;

  getTouchRaw(&x_tmp, &y_tmp);


  delay(1); // Small delay to the next sample
  if (getTouchRawZ() <= threshold) return false;

  delay(2); // Small delay to the next sample
  getTouchRaw(&x_tmp2, &y_tmp2);


  if (abs(x_tmp - x_tmp2) > _RAWERR) return false;
  if (abs(y_tmp - y_tmp2) > _RAWERR) return false;

  *x = x_tmp;
  *y = y_tmp;

  return true;
}
void touch_calibrate()//屏幕校准
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;
  Serial.println("屏幕校准");

  //校准
  //  lcd.fillScreen(TFT_BLACK);
  //  lcd.setCursor(20, 0);
  //  Serial.println("setCursor");
  //  lcd.setTextFont(2);
  //  Serial.println("setTextFont");
  //  lcd.setTextSize(1);
  //  Serial.println("setTextSize");
  //  lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  //  lcd.println("按指示触摸角落");
  Serial.println("按指示触摸角落");
  //  lcd.setTextFont(1);
  //  lcd.println();
  //      lcd.setCursor(175, 100);
  //      lcd.printf("Touch Adjust");
  //  Serial.println("setTextFont(1)");
  lv_timer_handler();
  calibrateTouch(calData, TFT_RED, TFT_BLACK, 19);
  Serial.println("calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15)");
  Serial.println(); Serial.println();
  Serial.println("//在setup()中使用此校准代码:");
  Serial.print("uint16_t calData[5] = ");
  Serial.print("{ ");

  for (uint8_t i = 0; i < 5; i++)
  {
    Serial.print(calData[i]);
    if (i < 4) Serial.print(", ");
  }

  Serial.println(" };");
  //  Serial.print("  tft.setTouch(calData);");
  //  Serial.println(); Serial.println();
  //  lcd.fillScreen(TFT_BLACK);
  //  lcd.println("XZ OK!");
  //  lcd.println("Calibration code sent to Serial port.");


}
void calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size) {
  int16_t values[] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint16_t x_tmp, y_tmp;
  uint16_t _width = 800;
  uint16_t _height = 480;

  for (uint8_t i = 0; i < 4; i++) {
    lcd.fillRect(0, 0, size + 1, size + 1, color_bg);
    lcd.fillRect(0, _height - size - 1, size + 1, size + 1, color_bg);
    lcd.fillRect(_width - size - 1, 0, size + 1, size + 1, color_bg);
    lcd.fillRect(_width - size - 1, _height - size - 1, size + 1, size + 1, color_bg);

    if (i == 5) break; // used to clear the arrows

    switch (i) {
      case 0: // up left
        lcd.drawLine(0, 0, 0, size, color_fg);
        lcd.drawLine(0, 0, size, 0, color_fg);
        lcd.drawLine(0, 0, size , size, color_fg);
        break;
      case 1: // bot left
        lcd.drawLine(0, _height - size - 1, 0, _height - 1, color_fg);
        lcd.drawLine(0, _height - 1, size, _height - 1, color_fg);
        lcd.drawLine(size, _height - size - 1, 0, _height - 1 , color_fg);
        break;
      case 2: // up right
        lcd.drawLine(_width - size - 1, 0, _width - 1, 0, color_fg);
        lcd.drawLine(_width - size - 1, size, _width - 1, 0, color_fg);
        lcd.drawLine(_width - 1, size, _width - 1, 0, color_fg);
        break;
      case 3: // bot right
        lcd.drawLine(_width - size - 1, _height - size - 1, _width - 1, _height - 1, color_fg);
        lcd.drawLine(_width - 1, _height - 1 - size, _width - 1, _height - 1, color_fg);
        lcd.drawLine(_width - 1 - size, _height - 1, _width - 1, _height - 1, color_fg);
        break;
    }

    // user has to get the chance to release
    if (i > 0) delay(1000);

    for (uint8_t j = 0; j < 8; j++) {
      while (touch_has_signal())
      {
        if (touch_touched())
        {
          Serial.print( "Data x :" );
          Serial.println( touch_last_x );
          Serial.print( "Data y :" );
          Serial.println( touch_last_y );
          break;
        }
      }
    }
  }
}



void setTouch(uint16_t *parameters) {
  touchCalibration_x0 = parameters[0];
  touchCalibration_x1 = parameters[1];
  touchCalibration_y0 = parameters[2];
  touchCalibration_y1 = parameters[3];

  if (touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if (touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if (touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if (touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  touchCalibration_rotate = parameters[4] & 0x01;
  touchCalibration_invert_x = parameters[4] & 0x02;
  touchCalibration_invert_y = parameters[4] & 0x04;
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


  //BLE
  BLEDevice::init(bleServerName);  //创建BLE并设置名称
  pServer = BLEDevice::createServer();  //创建BLE服务器
  pServer->setCallbacks(new MyServerCallbacks());  //设置连接和断开调用类
  pService = pServer->createService(SERVICE_UUID); //创建BLE服务

  pCharacteristic = pService->createCharacteristic(  //Create ble feature（Characterristic_UUID）
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setValue("ELECROW");
  //开始广播
  pAdvertising = BLEDevice::getAdvertising();  //A bleadvertising class pointer padvertising is defined, which points to bledevice:: getadvertising()
  pAdvertising->addServiceUUID(SERVICE_UUID);


  size_t freeHeapSize = esp_get_free_internal_heap_size();
  // 打印结果到串口
  Serial.print("LVGL初始化之前Free Internal Heap Size: ");
  Serial.print(freeHeapSize);
  Serial.println(" bytes"); 
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
  while (1)
  {
    if (goto_widget_flag == 1)//进入widget
    {
      if (ticker1.active() == true)
      {
        ticker1.detach();
      }
      goto_widget_flag = 0;
      delay(300);
      break;
    }

    if (goto_widget_flag == 3)//进入触摸界面，先把进度条线程关闭
    {
      bar_flag = 0; //停止进度条标志
      if (ticker1.active() == true)
      {
        ticker1.detach();
      }
      if (first_flag == 0 || first_flag == 1)
      {
        label_xy();
        first_flag = 2;
      }
      if (zero_clean == 1)
      {
        touch_last_x = 0;
        touch_last_y = 0;
        zero_clean = 0;
      }
      lv_label_set_text(ui_Label, "Touch Adjust:");
      lv_label_set_text_fmt(ui_Label3, "%d  %d", touch_last_x, touch_last_y); //显示触摸信息
    }

    if (goto_widget_flag == 4)//触摸界面返回到Menu界面,使进度条清零重启
    {
      val = 100;
      delay(100);
      ticker1.attach_ms(35, callback1);//每20ms调用callback1
      goto_widget_flag = 0;
    }

    if (goto_widget_flag == 5) //触发校准信号
    {
      lv_scr_load_anim(ui_touch_calibrate, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
      lv_timer_handler();
      lv_timer_handler();
      delay(100);
      touch_calibrate();//触摸校准
      lv_scr_load_anim(ui_TOUCH, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
      lv_timer_handler();
      delay(100);
      goto_widget_flag = 3; //进入触摸界面标志
      touch_last_x = 0;
      touch_last_y = 0;
    }

    if (bar_flag == 6)//刚开机进入Menu界面时运行进度条一次，之后就不再运行
    {
      if (first_flag == 0)
      {
        lv_example_bar();
        ticker1.attach_ms(35, callback1);//每20ms调用callback1
        first_flag = 1;
      }
    }

    lv_timer_handler();
  }


  lcd.fillScreen(TFT_BLACK);
  lv_demo_widgets();//主UI界面
  Serial.println( "Setup done" );
  //  }

}

void loop()
{
  while (NO_Test_Flag == 0)
  {
    lv_timer_handler();
    CloseData = Serial.read();
    if (CloseData == 'b')  //Exit
    {
      NO_Test_Flag = 1;
      Test_Flag = 1;
      lcd.fillScreen(TFT_BLACK);
      Serial.println("进入测试程序");
      WiFi.disconnect();
      break;
    }
  }

  //*********************************测试程序***************************************
  //*********************************测试程序***************************************
  //*********************************测试程序***************************************
  while (Test_Flag == 1)
  {
    Ce_shi();
    
  }
  ESP.restart();
}
