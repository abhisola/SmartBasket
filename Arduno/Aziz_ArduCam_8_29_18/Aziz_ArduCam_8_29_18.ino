#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>

//needed for wifiManager library
#include <DNSServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <EEPROM.h>
#include "memorysaver.h"
#if !(defined ESP8266 )
#error Please select the ArduCAM ESP8266 UNO board in the Tools/Board
#endif

//This demo can only work on OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED
//or OV5640_MINI_5MP_PLUS or ARDUCAM_SHIELD_V2 platform.
#if !(defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) || defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

#define LEDpin 0
#define DONEpin 15

// set GPIO16 as the slave select :
float batteryVoltage = 0;
const int CS = 16;
const byte sleepTime_hr = 3;// 3 = will sleep at <1uA for 3hrs
const int httpPort = 80; // Host Port 80 if on heroku
//const int httpPort = 3000; // Host Port 80 if on heroku
const char* host = "smart-basket-api.herokuapp.com"; // Host Name heroku App Name
//const char* host = "192.168.0.104";
String url = "/smartbasket/api/1/1"; // /smartbasket/api/StoreId/BasketId

ArduCAM myCAM(OV5642, CS);
void start_capture() {
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}
void camCapture(ArduCAM myCAM) {
  size_t len = myCAM.read_fifo_length();
  if (len >= MAX_FIFO_SIZE) {
    Serial.println("Over size.");
    return;
  } else if (len == 0 ) {
    Serial.println("Size is 0.");
    return;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
#if !(defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
  SPI.transfer(0xFF);
#endif
  // Prepare request
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  if (client.connected()) {

    client.print("POST ");
    client.print(url);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.print(host);
    client.print(":");
    client.println(httpPort);
    client.print("Content-Length: "); client.println(len);
    client.println("Content-Type: image/jpeg");
    client.println("Content-Transfer-Encoding: binary");
    client.println();

    //static const size_t bufferSize = 4096;
    static const size_t bufferSize = 2000;
    static uint8_t buffer[bufferSize] = {0xFF};
Serial.println(len);
    while (len) {
      size_t n = (len < bufferSize) ? len : bufferSize;
      myCAM.transferBytes(&buffer[0], &buffer[0], n);
      if (!client.connected()) {
        Serial.println("connection lost!");
        break;
      }
      client.write(&buffer[0], n);
      len -= n;
      Serial.print(".");
      yield();
    }
  } else {
    Serial.println(F("Connection failed"));
  }
  myCAM.CS_HIGH();
  Serial.println("done");
  client.stop();

  batteryVoltage = 0.00478 * analogRead(A0) - 0.015;//scale that 1V max to 0-4.5V
}
void serverCapture() {
  start_capture();
  Serial.println("CAM Capturing");

  int total_time = 0;
  total_time = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    yield();
  }
  total_time = millis() - total_time;
  Serial.print("capture total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  total_time = 0;
  Serial.println("CAM Capture Done!");
  total_time = millis();
  camCapture(myCAM);
  total_time = millis() - total_time;
  Serial.print("send total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  Serial.println("CAM send Done!");
}
void setup() {
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
  pinMode(DONEpin, OUTPUT);

  EEPROM.begin(4);
  byte wakeCount = EEPROM.read(0);
  Serial.println(wakeCount);
  if (wakeCount < (sleepTime_hr - 1)) {
    wakeCount++;
    EEPROM.write(0, wakeCount);
    EEPROM.end();
    while (1) {
      digitalWrite(DONEpin, HIGH);
      delay(100);
      digitalWrite(DONEpin, LOW);
      delay(100);
    }
  }
  else {
    wakeCount = 0;
    EEPROM.write(0, wakeCount);
    EEPROM.end();
  }
  pinMode(CS, INPUT);
  while (digitalRead(CS) == LOW) {
    yield(); // wait for the external pin to go LOW - could still be HIGH, so gotta wait
  }

  //delay(100);
  uint8_t vid, pid;
  uint8_t temp;
#if defined(__SAM3X8E__)
  Wire1.begin();
#else
  Wire.begin();
#endif
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the CS as an output:
  digitalWrite(CS, HIGH);
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55) {
    Serial.println("SPI1 interface Error!");
    while (1);
  }
  //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  if ((vid != 0x56) || (pid != 0x42)) {
    Serial.println("Can't find OV5642 module!");
    while (1);
  }
  else
    Serial.println("OV5642 detected.");

  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_1280x960);
  delay(1100);
  WiFiManager wifiManager;
  wifiManager.setTimeout(120);//2min, if can't get connected just go back to sleep
  if (!wifiManager.autoConnect("trigBoard")) {// SSID you connect to from browser
    Serial.println("failed to connect and hit timeout");
    digitalWrite(DONEpin, HIGH);
    //return 0;
  }


  serverCapture();
  //  //ESP.deepSleep(1.08e10); // Uncoment this for 3 hour sleep mode
  //  ESP.deepSleep(60e6); // Uncoment this for 60 second sleep mode
}
void loop() {
  digitalWrite(DONEpin, HIGH);
  delay(100);
  digitalWrite(DONEpin, LOW);
  delay(100);
}

