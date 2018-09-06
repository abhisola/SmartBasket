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
const int CS = 16;

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
  String start_request = "";
  String end_request = "";
  start_request = start_request + "\n" + "--AaB03x" + "\n" + "Content-Disposition: form-data; name=\"picture\"; filename=\"CAM.JPG\"" + "\n" + "Content-Type: image/jpeg" + "\n" + "Content-Transfer-Encoding: binary" + "\n" + "\n";
  end_request = end_request + "\n" + "--AaB03x--" + "\n";
  uint16_t extra_length;
  extra_length = start_request.length() + end_request.length();
  uint16_t fulllen = len + extra_length;
  Serial.println("Full request:");
  Serial.println(F("POST /pringles/img HTTP/1.1"));
  Serial.println(F("Host: 192.168.0.104:3000"));
  Serial.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
  Serial.print(F("Content-Length: "));
  Serial.println(fulllen);
  Serial.print(start_request);
  Serial.print("binary data");
  Serial.print(end_request);
  WiFiClient client;
  const int httpPort = 3000;
  const char* host = "192.168.0.104";
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  if (client.connected()){
    client.println(F("POST /pringles/img HTTP/1.1"));
    client.println(F("Host: 192.168.0.104:3000"));
    client.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
    client.print(F("Content-Length: "));
    client.println(fulllen);
    client.print(start_request);

    static const size_t bufferSize = 4096;
    static uint8_t buffer[bufferSize] = {0xFF};
    byte wCount = 0; // For counting # of writes
    while (len) {
        size_t will_copy = (len < bufferSize) ? len : bufferSize;
        myCAM.transferBytes(&buffer[0], &buffer[0], will_copy);
        if (!client.connected()) break;
        client.write(&buffer[0], will_copy);
        if(++wCount >= 64) { // Every 2K, give a little feedback so it doesn't appear locked up
          Serial.print('.');
          wCount = 0;
         }
        len -= will_copy;
    }
    client.print(end_request);
    client.println();
    Serial.print('Number Of Writes: ');
    Serial.println(wCount);
  } else {
     Serial.println(F("Connection failed"));
  }
  while (client.connected()) {

      while (client.available()) {

    // Read answer
        char c = client.read();
        Serial.print(c);
        //result = result + c;
      }
    }
    //client.close();
  myCAM.CS_HIGH();

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
      myCAM.OV5642_set_JPEG_size(OV5642_320x240);
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
}
void loop() {

}
