#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>
#include <XPowersLib.h>
#include <ESP32Time.h>
#include <WiFi.h>
#include <esp_now.h>
#include "time.h"
#include <TFT_eSPI.h>
#include <TouchDrvCSTXXX.hpp>
#include <SensorLTR553.hpp>
#include "utilities.h"
#include "font.h"
#include "smallFont.h"
#include "middleFont.h"

uint8_t broadcastAddress[] = {0x34, 0x85, 0x18, 0x8B, 0x64, 0x5C};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int a;
  int b;
} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite barSpr = TFT_eSprite(&tft);
TFT_eSprite swSpr = TFT_eSprite(&tft);
TFT_eSprite clSpr = TFT_eSprite(&tft);
TFT_eSprite daSpr = TFT_eSprite(&tft);

const char* ssid     = ""; ///..................................edit this
const char* password = ""; ///..................................edit this
int8_t broadcastAddress[] = {0x00, 0x00, 0x00, 0x00, 0x64, 0x00}; //.............esit this
String ip=""; 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec =3600;            //time zone * 3600 , my time zone is  +1 GTM
const int   daylightOffset_sec = 0; 


PowersSY6970 PMU;
SensorLTR553 als;
TouchDrvCSTXXX touch;
int16_t x[5], y[5];

unsigned short grays[15];
unsigned short blue=0x09A9;
int barValue=4;

unsigned long timePased=0;
int updatePeriod=500;

    uint16_t ch0 = 0;
    uint16_t ch1 = 0;
    uint16_t ps = 0;
    bool saturated;
  
ESP32Time rtc(0); 
String tmStr="";
unsigned short swC[2]={0x10A2,TFT_ORANGE};

bool switchs[2]={0};
String states[2]={"OFF","ON"};
int deb=0;

void getTime()    // get time from server
  {

   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
   delay(500);}

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    rtc.setTimeStruct(timeinfo); 
  }

  
 }

void setBrightness(uint8_t value)
{
    static uint8_t level = 0;
    static uint8_t steps = 16;
    if (value == 0) {
        digitalWrite(BOARD_TFT_BL, 0);
        delay(3);
        level = 0;
        return;
    }
    if (level == 0) {
        digitalWrite(BOARD_TFT_BL, 1);
        level = steps;
        delayMicroseconds(30);
    }
    int from = steps - level;
    int to = steps - value;
    int num = (steps + to - from) % steps;
    for (int i = 0; i < num; i++) {
        digitalWrite(BOARD_TFT_BL, 0);
        digitalWrite(BOARD_TFT_BL, 1);
    }
    level = value;
}


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void touchHomeKeyCallback(void *user_data)
{
   delay(1);
}

void setup()
{

    tft.init();
    tft.fillScreen(TFT_BLACK);

    // Initialize capacitive touch
    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_IRQ);
    touch.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, CST226SE_SLAVE_ADDRESS);

    //Set the screen to turn on or off after pressing the screen Home touch button
    touch.setHomeButtonCallback(touchHomeKeyCallback);
   
    barSpr.createSprite(50, 280);
    swSpr.createSprite(200, 80);
    clSpr.createSprite(200, 70);
    daSpr.createSprite(140, 280);

    barSpr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    barSpr.setTextDatum(4);
    pinMode(BOARD_TFT_BL, OUTPUT);

    
    getTime();
    ip=WiFi.localIP().toString();
    delay(20);
    WiFi.disconnect();

    WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

    PMU.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, SY6970_SLAVE_ADDRESS);
    PMU.enableADCMeasure();

    pinMode(BOARD_SENSOR_IRQ, INPUT_PULLUP);
    als.begin(Wire, LTR553_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL);

    als.setLightSensorThreshold(10, 200);
    als.setLightSensorPersists(1);
    als.setProximityPersists(1);
    als.setIRQLevel(SensorLTR553::ALS_IRQ_ACTIVE_LOW);
    als.enableIRQ(SensorLTR553::ALS_IRQ_BOTH);
    als.setLightSensorGain(SensorLTR553::ALS_GAIN_1X);
    als.setPsLedPulsePeriod(SensorLTR553::PS_LED_PLUSE_100KHZ);
    als.setPsLedDutyCycle(SensorLTR553::PS_LED_DUTY_100);
    als.setPsLedCurrnet(SensorLTR553::PS_LED_CUR_100MA);
    als.setProximityRate(SensorLTR553::PS_MEAS_RATE_50MS);
    als.setPsLedPulses(1);
    als.enableLightSensor();
    als.enableProximity();
    als.enablePsIndicator();

     int co=225;
       for(int i=0;i<15;i++)
        {
          grays[i]=tft.color565(co, co, co);
          co=co-15;
        }
     setBrightness(4);
     drawBar();  
     drawSw(); 
     drawCl();
     drawDa();

}

void drawBar()
{
  barSpr.fillSprite(TFT_BLACK);
  barSpr.drawRect(0,0,50,280,grays[8]);
  barSpr.loadFont(smallFont);
  barSpr.setTextColor(grays[5],TFT_BLACK);
  barSpr.drawString("BLIGHT", 25, 15, 2);
  barSpr.unloadFont();
  barSpr.loadFont(middleFont);
  barSpr.setTextColor(grays[4],TFT_BLACK);
  barSpr.drawString(String(barValue), 25, 46, 4);
  barSpr.unloadFont();

    for(int i=0;i<16;i++)
    barSpr.fillRect(4,280-(i*14),42,9,grays[14]);
       
    for(int i=0;i<barValue;i++)
    barSpr.fillRect(4,280-(i*14),42,9,TFT_ORANGE);
    barSpr.pushSprite(160, 90);
}

void drawSw()
  {

    swSpr.fillSprite(TFT_BLACK);
    swSpr.fillRoundRect(0,0,95,80,4,swC[switchs[0]]);
    swSpr.fillRoundRect(105,0,95,80,4,swC[switchs[1]]);
    swSpr.drawArc(47, 42, 26, 23, 0, 150,swC[!switchs[0]], swC[switchs[0]]);
    swSpr.drawArc(47, 42, 26, 23, 210, 140,swC[!switchs[0]], swC[switchs[0]]);
    swSpr.fillRect(45,10,5,24,swC[!switchs[0]]);

        swSpr.drawArc(153, 42, 26, 23, 0, 150,swC[!switchs[1]], swC[switchs[1]]);
        swSpr.drawArc(152, 42, 26, 23, 210, 140,swC[!switchs[1]], swC[switchs[1]]);
        swSpr.fillRect(150,10,5,24,swC[!switchs[1]]);

    
    swSpr.loadFont(smallFont);
    swSpr.setTextColor(swC[!switchs[0]], swC[switchs[0]]);
    swSpr.drawString(states[switchs[0]],10,5);
    swSpr.setTextColor(swC[!switchs[1]], swC[switchs[1]]);
    swSpr.drawString(states[switchs[1]],115,5);
    swSpr.unloadFont();
    swSpr.pushSprite(10,380);
  }

  void drawCl()
  {
    clSpr.fillSprite(TFT_BLACK);
    clSpr.loadFont(bigFont);
    clSpr.setTextColor(grays[3],TFT_BLACK);
    clSpr.drawString(rtc.getTime().substring(0,5),0,0);
    clSpr.unloadFont();
    clSpr.loadFont(smallFont);
    clSpr.setTextColor(TFT_ORANGE,TFT_BLACK);
    clSpr.drawString(rtc.getDate(true),0,56,2);
    clSpr.unloadFont();
    clSpr.loadFont(middleFont);
    clSpr.setTextColor(grays[4],TFT_BLACK);
    clSpr.drawString(rtc.getTime().substring(6,9),114,0);
    clSpr.unloadFont();
    clSpr.setTextColor(0x3DB9,TFT_BLACK);
    clSpr.drawString("SET_TIME",164,0);
    clSpr.fillSmoothCircle(194, 44, 1, grays[9]);
    clSpr.drawArc(194, 44, 7, 6, 90, 180, grays[9], TFT_BLACK);
    clSpr.drawArc(194, 44, 14, 13, 90, 180, grays[9], TFT_BLACK);
    clSpr.drawArc(194, 44, 21, 20, 90, 180, grays[9], TFT_BLACK);
    clSpr.drawArc(194, 44, 28, 27, 90, 180, grays[9], TFT_BLACK);

    clSpr.pushSprite(10,10);
  }

  void drawDa()
  {
    daSpr.fillSprite(grays[14]);
    daSpr.loadFont(smallFont);
    daSpr.setTextDatum(0);
    daSpr.setTextColor(grays[1],grays[14]);
    daSpr.drawString("BATTERY    USB",10,10);
    

    daSpr.fillSmoothRoundRect(10,30,55,35,3,grays[11],grays[14]);
    daSpr.fillSmoothRoundRect(75,30,55,35,3,grays[11],grays[14]);
    daSpr.unloadFont();

    daSpr.loadFont(middleFont);
    daSpr.setTextDatum(4);
    daSpr.setTextColor(grays[1],grays[11]);
    daSpr.drawString(String(PMU.getBattVoltage()/1000.00),37,50);
    daSpr.drawString(String(PMU.getVbusVoltage()/1000.00),102,50);
    daSpr.setTextColor(grays[3],grays[14]);
    daSpr.setTextDatum(0);
    daSpr.drawString("WIFI:",10,170);
    daSpr.unloadFont();

    
    daSpr.loadFont(smallFont);
    daSpr.setTextColor(grays[5],grays[14]);
    daSpr.drawString("LIGHT SENSOR1:",10,80);
    daSpr.drawString("LIGHT SENSOR2:",10,105);
    daSpr.drawString("PROXIMIDITY:",10,130);
    daSpr.setTextColor(grays[6],grays[14]);
    daSpr.drawString(ssid,44,205);
    daSpr.drawString(String(WiFi.macAddress()),40,230);
    daSpr.drawString(String(ip),30,255);

    daSpr.setTextColor(TFT_ORANGE,grays[14]);
    daSpr.drawString(String(ch0),110,80);
    daSpr.drawString(String(ch1),110,105);
    daSpr.drawString(String(ps),110,130);
    daSpr.drawString("SSID:",10,205);
    daSpr.drawString("MAC:",10,230);
    daSpr.drawString("IP:",10,255);
    daSpr.unloadFont();

    daSpr.drawWedgeLine(116,280,140,256,1,1,0x3DB9,grays[14]);
    daSpr.drawWedgeLine(124,280,140,264,1,1,0x3DB9,grays[14]);
    daSpr.drawWedgeLine(132,280,140,272,1,1,0x3DB9,grays[14]);
    daSpr.pushSprite(10,90);

  }

void loop()
{
    if(tmStr!=rtc.getTime())
    {
      tmStr=rtc.getTime();
      drawCl();
    }
    if(timePased+updatePeriod<millis())
    {
        timePased=millis();
        ch0 = als.getLightSensor(0);
        ch1 = als.getLightSensor(1);
        ps = als.getProximity(&saturated);
        drawDa();
    }
  
    uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
    if (touched) {
      if(x[0]>160 && y[0]>150 && y[0]<370)
      {barValue=map(y[0],370,160,0,16);
       setBrightness(barValue);
       drawBar(); }

       if(y[0]>380 && y[0]<480){
        if(deb==0)
        {
          deb=1;
          if(x[0]>110){
            switchs[1]=!switchs[1];
            myData.a=1;
            myData.b=switchs[1];
             esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
          }
          else{
            switchs[0]=!switchs[0];
            myData.a=0;
            myData.b=switchs[0];
             esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

          }

          drawSw();}
       }
    }else deb=0;
 
}

  /*
    bool switchs[2]={0};
    String states[2]={"OFF","ON"};
    */

