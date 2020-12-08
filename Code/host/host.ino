
#include <SPI.h>
#include <RH_RF95.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Wire.h>
#include "SparkFun_VL53L1X.h" //Click here to get the library: http://librarymanager/All#SparkFun_VL53L1X
#define timing 10

SFEVL53L1X distanceSensor;
RH_RF95 rf95;

int led = 9;
unsigned long last = 0;

void SDinit() {
  if (!SD.begin(4)) {
    Serial.println("Card Mount Failed");
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
}

void VL53init() {
  if (distanceSensor.begin() != 0) //Begin returns 0 on a good init
  {
    Serial.println("Sensor failed to begin. Please check wiring. Freezing...");
    while (1)
      ;
  }
  Serial.println("Sensor online!");

  distanceSensor.setDistanceModeShort();
}

void setup()
{
  pinMode(led, OUTPUT);
  Wire.begin();
  Serial.begin(115200);
  delay(100);

  SDinit();

  if (!rf95.init())
    Serial.println("init failed");
  rf95.setTxPower(20, false);

  //  saveToSD(0x03, 123.2, 1);

  VL53init();
}

void loop() {
  if (millis() - last > timing*1000) { //wait 10 minutes
    last = millis();
    mesureDistance();
    sendDataRequest();
  }
  lora();
  // ESP.deepSleep(10e6);
}

void mesureDistance() {
  distanceSensor.startRanging(); //Write configuration bytes to initiate measurement

  while (!distanceSensor.checkForDataReady())
  {
    delay(1);
  }
  float distance = distanceSensor.getDistance(); //Get the result of the measurement from the sensor
  distanceSensor.clearInterrupt();
  distanceSensor.stopRanging();
  saveToSD(0, distance / 10, InternalBatt());
}

bool InternalBatt() {
  //TODO
  return 0;
}

void sendDataRequest() {
  uint8_t duta[] = "B";
  rf95.send(duta, sizeof(duta));
  rf95.waitPacketSent();
}

void lora() {
  if (rf95.available())
  {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len))
    {
      digitalWrite(led, HIGH);
      if (len == 4) {
        float i = ((buf[1] << 8) + buf[2]);
        bool bat = 0;
        i = (i == 65526) ? -1 : i / 10;
        Serial.printf("Got %3.1fcm. from device: %d. RSSI: %d", i, buf[0], rf95.lastRssi());
        if (buf[3] == 'q') {
          bat = 1;
          Serial.print(" low Battery");
        }else{
          
        }
        Serial.println("");
        saveToSD(buf[0], i, bat);
        uint8_t data[] = "A";
        rf95.send(data, sizeof(data));
        rf95.waitPacketSent();
        digitalWrite(led, LOW);
      }
    }
    else
    {
      Serial.println("recv failed");
    }
  }
}

void saveToSD(uint8_t Addr, float Data, bool LowBat) {
  char path[15];
  sprintf(path, "/ID_%u.CSV", Addr);
  char text[20];
  char B = LowBat ? 'Y' : 'N';
  sprintf(text, "\n%lu;%3.1f;%c", millis(), Data, B);
  if (testFileName(SD, path)) {
    writeFile(SD, path, "Tid;afstand (cm);Low Battery?");
  }
  appendFile(SD, path, text);
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

bool testFileName(fs::FS &fs, const char * dirname) {
  File root = fs.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return 1;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return 1;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      if (!strcmp(file.name(), dirname)) {
        return 0;
      }
    }
    file = root.openNextFile();
  }
  return 1;
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (!file.print(message)) {
    Serial.println("Append failed");
  }
  file.close();
}
