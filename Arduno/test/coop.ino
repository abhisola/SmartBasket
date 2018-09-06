

//Hookup notes
// By James Eckert adapted form many sources from Arducam Examples and HTTP Post was fixed with code in forum question by AndreasGabler https://forum.arduino.cc/index.php?topic=348012.0
// For Use with ArduCAM_Mini_OV2640
// Runs on 3AA Nickle Metal hydride It is over power on the ESP8266 but cam needs extra power going through Mosfet
// Esp seems ok with higher voltage of course it is only on for about 30sec at a time
// ESP hookup
//   Standard ESP connection as well as  GPIO16--1K--RST  ( for Deep Sleep Control)
//   Light Sensor Vcc--1K--CDS-(A0)-1K--GND
// ArduCAM_Mini Cam hookup
//   CAM Power control GPIO5 to Gate of BS170 Mosfet, Arducam Gound to Drain and Source to Ground
//   SDA=GPIO0  SDL=GPIO2
//   CS=GPIO4  MOSI=GPIO13 SCK=GPIO14 MOS0=GPIO12
//Operation
// -wakes every 270 seconds.
// -check how bright it is
// -if dark goes back to deep sleep for 15min.
// -if not dark turns on the camera with a mosfet on its GND side of Cam
// -connects to the wireless I connection fail go back to sleep
// -takes a picture and posts it to my home webserver from the cameras fifo.
// -goes back to deep sleep for 270 seconds.


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"

// Enabe debug tracing to Serial port.
#define DEBUGGING

// Here we define a maximum framelength to 64 bytes. Default is 256.
#define MAX_FRAME_LENGTH 64

// Define how many callback functions you have. Default is 1.
#define CALLBACK_FUNCTIONS 1

// set GPIO4 as the slave select :
const int CS = 4;
int LightLevel=0;
int bfcnt=1;
int wfcnt=0;
const char* ssid = "YourSsid"; // Put your SSID here
const char* password = "YourKey"; // Put your PASSWORD here

ArduCAM myCAM(OV2640, CS);

void start_capture(){
  myCAM.clear_fifo_flag();
  Serial.println("Slight Pause");
  delay(3500);  //Give cam more time to adjust to light
  myCAM.start_capture();
}

void camCapture(ArduCAM myCAM){
  WiFiClient client ;

  uint8_t temp, temp_last;
  static int i = 0;
  static uint8_t first_packet = 1;
  byte buf[2048];
  uint32_t length = 0;
  length = myCAM.read_fifo_length();

  size_t len = myCAM.read_fifo_length();
  if (len >= 393216){
    Serial.println("Over size.");
    return;
  }else if (len == 0 ){
    Serial.println("Size is 0.");
    return;
  }

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  SPI.transfer(0xFF);

  ////////////////////////////////
  // Prepare HTTP Post request
  ////////////////////////////////

 if (client.connect("10.16.10.11", 8980)) {
    Serial.println("Start uploading...");
  String start_request = "";
  String end_request = "";
  String filename="test.jpg";

  start_request = start_request + "\n" + "--AaB03x" + "\n" + "Content-Disposition: form-data; name=\"fileToUpload\";filename="+filename+"\n" + "Content-Type: file" + "\n" + "Content-Transfer-Encoding: binary" + "\n" + "\n";
  end_request = end_request + "\n" + "--AaB03x--" + "\n";
  len=len+start_request.length();
  len=len+end_request.length();
  client.println("POST /tempgr/upload.php?LightLevel="+String(LightLevel)+"  HTTP/1.1");
  client.println(F("Host: 10.16.10.11"));
  client.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
  client.print(F("Content-Length: "));
  client.println(String(len));
  client.print(start_request);

  Serial.println(F("POST /tempgr/upload.php HTTP/1.1"));
  Serial.println(F("Host: 10.16.10.11"));
  Serial.println(F("Content-Type: multipart/form-data; boundary=AaB03x"));
  Serial.print(F("Content-Length: "));
  Serial.println(String(len));
  Serial.print(start_request);
  Serial.println("#################################################");

//////////////////////////////////////

while ( (temp != 0xD9) | (temp_last != 0xFF)) {
   //Serial.println(i,DEC);
   // yield();
    temp_last = temp;
    temp = SPI.transfer(0x00);
    //Write image data to buffer if not full
    // yield();
    if (i < 2048)
    {
      buf[i++] = temp;
    }
    else
    {
      if (first_packet == 1)
      {
        Serial.print("Step One:");
        Serial.println(i,DEC);
        client.write((char*)buf, 2048);
        first_packet = 0;
       // yield();
      }
      else
      {
        Serial.print("Step Two:");
        Serial.println(i,DEC);
        client.write((char*)buf, 2048);
        //yield();
      }
      i = 0;
      buf[i++] = temp;
      }
  }
  //Write the remain bytes in the buffer
    Serial.print("leftover:");
    Serial.println(i,DEC);

  if (i > 0)
  {
    Serial.print("finish write last");
    Serial.println(i,DEC);
    client.write((char*)buf, i);
    i = 0;
    first_packet = 1;
  }
    Serial.print("leftover:");
    Serial.println(i,DEC);
  //////////////////////////////////////////////////////////////////////
  client.println(end_request);
  Serial.println(end_request);
  myCAM.CS_HIGH();
 }
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
  delay(500);
  digitalWrite(5, LOW);
  delay(1000);
  SleepyTime(270);
  }

void SleepyTime(int slsec ) {
  Serial.println("Time:"+String(millis()));
  Serial.println("Going to Sleep For:"+String(slsec));
  ESP.deepSleep(slsec * 1000000,WAKE_NO_RFCAL);
  //ESP.deepSleep(30 * 1000000,WAKE_NO_RFCAL);
}

void setup() {
  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin(0,2);
  Serial.begin(115200);
  Serial.println("ArduCAM Post Start!");
  pinMode(5, OUTPUT);
  LightLevel = analogRead(A0);
  Serial.println("ADC:"+String(LightLevel));
  if ( LightLevel<350 ) {  // if it is to dark go back to sleep
    digitalWrite(5, LOW);
    SleepyTime(900);
  }
  digitalWrite(5, HIGH);   // Turn on cam Drives Mosfet connected to Cameras Ground (BS170 MOSFET N-CHANNEL)
  delay(200);
  // set the CS as an output:
  pinMode(CS, OUTPUT);
  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //8MHz
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
    Serial.println("SPI1 interface Error!");
    while(1);
  }

  //Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  //myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  //myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

  // if cam was used in lowpower/powerdown mode it needs to be cleared (I had server version of this sketch that uses it)
  // myCAM.clear_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);
  // myCAM.clear_bit(ARDUCHIP_TIM,FIFO_PWRDN_MASK);
  // myCAM.clear_bit(ARDUCHIP_TIM,LOW_POWER_MODE);

  //Change to JPEG capture mode and initialize the OV2640 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_640x480);
  myCAM.clear_fifo_flag();
  myCAM.write_reg(ARDUCHIP_FRAMES, 0x00);

  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  yield();
  /////////////////////////
  // Connect to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //it connects faster with static IP
  IPAddress ip(10, 16, 10, 60);
  IPAddress gw(10, 16, 10, 1);
  IPAddress sbnm(255, 255, 255, 0);
  WiFi.config(ip, gw, gw, sbnm);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {  // try to connect wifi for 6 sec then reset
      delay(250);
      Serial.print(".");
      wfcnt++;
        if (wfcnt >25 ){
          SleepyTime(270); ;  //Wifi not connecting stop wasting bat and sleep
        }
     }
    Serial.println("WiFi connected");
    Serial.println("");
    Serial.println(WiFi.localIP());
  serverCapture();
}

void loop() {  // should never hit the loop but if does it will go to sleep for 5 min and try agian.
  if ( millis()>500000 ){
    SleepyTime(270);
  }
  yield();
  delay(6000);
}
