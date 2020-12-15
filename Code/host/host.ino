
#include <WiFi.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <RH_RF95.h>          //http://www.airspayce.com/mikem/arduino/RadioHead/index.html
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Wire.h>
#include "DS3231.h"
#define PollingIntervalSeconds (30*60)
#define Rled 27
#define Gled 25
#define Yled 12

RH_RF95 rf95;             //This defaults to use pins (CS = SPI_SS, Interupt pin = 2, SPI interface = VSPI).

RTClib RTC;

int led = 9;              //LED is used to see if we get a response from a node.
unsigned long last = 0;   //This is used to store the previous time, to calculate a delta time.
bool WasHereOnce = 0;
static bool hasSD = false;
File uploadFile;

void SDinit() {
  if (!SD.begin(4)) {     //initilize SD card width CS = 4.
    Serial.println("Card Mount Failed");
    return;
  } else hasSD = true;

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
}


void sendDataRequest() {
  uint8_t duta[] = "B";           //Makes an array consisting of B and a null termination.
  rf95.send(duta, sizeof(duta));  //Sends the B to all LoRa devices.
  rf95.waitPacketSent();          //Waits until any previous packet is finished transmitting.
}


void saveToSD() {
  char path[] = "test.CSV";                                          //make an array big enough to the path.
  //sprintf(path, "/ID_%u.CSV", Addr);                      //create the path string which includes the device ID, and saves into path array.
  char text[20];                                          //make an array big enough to hold the string we want to save to the SD card.
  //char B = LowBat ? 'Y' : 'N';                            //make B either Y or N, depending on low battery warning.
  sprintf(text, "\n%lu", RTC.now().unixtime());     //Create string ex. ("60000;25.2;Y";) and save it into text[]
  if (!SD.exists(path)) {                           //check if the file exist on the SD card. if not we want to create it.
    appendFile(SD, path, "Tid"); //Create a file and write the into the file. (the string is the CSV header).
  }
  appendFile(SD, path, text);                             //Append the text[] string to the path[] on the SD card.
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);     //open path
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.close();
}


void setup() {
  pinMode(led, OUTPUT);   //Sets LED to output.
  pinMode(Rled, OUTPUT);   //Sets LED to output.
  pinMode(Gled, OUTPUT);   //Sets LED to output.
  pinMode(Yled, OUTPUT);   //Sets LED to output.
  digitalWrite(Rled, 1);
  digitalWrite(Yled, 1);
  digitalWrite(Gled, 1);
  Wire.begin();           //initializes I2C.
  Serial.begin(115200);   //Initializes UART @ 115200 baud.
  delay(100);             //Waits to make sure UART is setup when we need it.

  SDinit();               //Initializes SD card

  if (!rf95.init()) Serial.println("init failed"); //Initializes LoRa moudule and warns us if init faliled.
  rf95.setTxPower(20, false);                      //20dbm (100mW) Configuration. theoretical 2x distance in comparison to 14dbm (25mW)

}

int low() {
  digitalWrite(Rled, 0);
  digitalWrite(Yled, 0);
  digitalWrite(Gled, 0);
}

char i = 0;
bool  good = false;

void loop() {
  sendDataRequest();
  uint8_t dat[] = "eh";
  int sz = sizeof(dat);
  rf95.send(dat, sz);
  rf95.waitPacketSent();
  
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.waitAvailableTimeout(3000))
  {
    if (rf95.recv(buf, &len))
    {
      Serial.print("got reply: ");
      Serial.println(buf[0]);
      if (dat[0] == buf[0] && dat[1] == buf[1] && len == sz) {
        good = true;
        Serial.println("Got ");
        Serial.println(buf[0]);
        low();
        digitalWrite(Gled, 1);
      }
    }
    else
    {
      if (good) {
        saveToSD();
        low();
        digitalWrite(Yled, 1);
      } else {
        low();
        digitalWrite(Rled, 1);
      }
      good = false;
      Serial.println("recv failed");
    }
  }
  else
  {
    if (good) {
      saveToSD();
      low();
      digitalWrite(Yled, 1);
    } else {
      low();
      digitalWrite(Rled, 1);
    }
    good = false;
    Serial.println("recv failed");
  }
  delay(500);
  i++;
}
