#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#if !(defined ESP8266 )
#error Please select the ArduCAM ESP8266 UNO board in the Tools/Board
#endif

//This demo can only work on OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED
//or OV5640_MINI_5MP_PLUS or ARDUCAM_SHIELD_V2 platform.
#if !(defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) || defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

// set GPIO16 as the slave select :
const int CS = 0;

//you can change the value of wifiType to select Station or AP mode.
//Default is AP mode.
int wifiType = 0; // 0:Station  1:AP

//AP mode configuration
//Default is arducam_esp8266.If you want,you can change the AP_aaid  to your favorite name
const char *AP_ssid = "arducam_esp8266";
//Default is no password.If you want to set password,put your password here
const char *AP_password = "";

//Station mode you should put your ssid and password
const char* ssid = "Awsm_Speed_Shop"; // Put your SSID here
const char* password = "bir7cha8su6"; // Put your PASSWORD here

ArduCAM myCAM(OV5642, CS);
void start_capture(){
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}
void camCapture(ArduCAM myCAM){

  size_t len = myCAM.read_fifo_length();
  if (len >= MAX_FIFO_SIZE){
    Serial.println("Over size.");
    return;
  }else if (len == 0 ){
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
  const int httpPort = 3000;
  const char* host = "192.168.0.104";
  String url = "/pringles/img";
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  if (client.connected()){

    client.print("POST ");
    client.print(url);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.print("Content-Length: "); client.println(len);
    client.println("Content-Type: image/jpeg");
    client.println("Content-Transfer-Encoding: binary");
    client.println();

    static const size_t bufferSize = 4096;
    static uint8_t buffer[bufferSize] = {0xFF};

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
    }
  } else {
     Serial.println(F("Connection failed"));
  }
  myCAM.CS_HIGH();
  Serial.println("done");
  client.stop();
}
void serverCapture(){
  start_capture();
  Serial.println("CAM Capturing");

  int total_time = 0;
  total_time = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
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
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
    Serial.println("SPI1 interface Error!");
    while(1);
  }
  //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x42)){
   Serial.println("Can't find OV5642 module!");
   while(1);
   }
   else
   Serial.println("OV5642 detected.");

     //Change to JPEG capture mode and initialize the OV5642 module
     myCAM.set_format(JPEG);
      myCAM.InitCAM();
      myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
      myCAM.OV5642_set_JPEG_size(OV5642_2592x1944);
      delay(1000);
     if (wifiType == 0){
       if(!strcmp(ssid,"SSID")){
          Serial.println("Please set your SSID");
          while(1);
       }
       if(!strcmp(password,"PASSWORD")){
          Serial.println("Please set your PASSWORD");
          while(1);
       }
       // Connect to WiFi network
       Serial.println();
       Serial.println();
       Serial.print("Connecting to ");
       Serial.println(ssid);

       WiFi.mode(WIFI_STA);
       WiFi.begin(ssid, password);
       while (WiFi.status() != WL_CONNECTED) {
         delay(500);
         Serial.print(".");
       }
       Serial.println("WiFi connected");
       Serial.println("");
       Serial.println(WiFi.localIP());
     }else if (wifiType == 1){
       Serial.println();
       Serial.println();
       Serial.print("Share AP: ");
       Serial.println(AP_ssid);
       Serial.print("The password is: ");
       Serial.println(AP_password);
       WiFi.mode(WIFI_AP);
       WiFi.softAP(AP_ssid, AP_password);
       Serial.println("");
       Serial.println(WiFi.softAPIP());
     }
     serverCapture();
     ESP.deepSleep(10e6);
}
void loop() {

}
