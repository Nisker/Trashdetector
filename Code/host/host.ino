
#include <SPI.h>
#include <RH_RF95.h>          //http://www.airspayce.com/mikem/arduino/RadioHead/index.html
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Wire.h>
#include "SparkFun_VL53L1X.h" //Click here to get the library: http://librarymanager/All#SparkFun_VL53L1X
#define PollingIntervalSeconds 10

SFEVL53L1X distanceSensor;//Defaults to (I2C @21+22).
RH_RF95 rf95;             //This defaults to use pins (CS = SPI_SS, Interupt pin = 2, SPI interface = VSPI).

int led = 9;              //LED is used to see if we get a response from a node.
unsigned long last = 0;   //This is used to store the previous time, to calculate a delta time.

void SDinit() {
  if (!SD.begin(4)) {     //initilize SD card width CS = 4.
    Serial.println("Card Mount Failed");
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
}

void VL53init() {
  if (distanceSensor.begin() != 0) {  //Begin returns 0 on a good init
    Serial.println("Sensor failed to begin. Please check wiring. Freezing...");
    while (1)                         //loops forever.
      ;
  }
  Serial.println("Sensor online!");
  distanceSensor.setDistanceModeShort();  //Sets the sensor to higher accuracy at short distance.
}

void setup() {
  pinMode(led, OUTPUT);   //Sets LED to output.
  Wire.begin();           //initializes I2C.
  Serial.begin(115200);   //Initializes UART @ 115200 baud.
  delay(100);             //Waits to make sure UART is setup when we need it.

  SDinit();               //Initializes SD card

  if (!rf95.init()) Serial.println("init failed"); //Initializes LoRa moudule and warns us if init faliled.
  rf95.setTxPower(20, false);                      //20dbm (100mW) Configuration. theoretical 2x distance in comparison to 14dbm (25mW)

  VL53init();             //Initializes ToF distance sensor.
}

void loop() {
  if (millis() - last > PollingIntervalSeconds*1000) {  //wait specified time in seconds.
    last = millis();                                    //Saves the current time to calculate delta time.
    mesureDistance();                                   //Locally mesures distance with ToF sensor.
    sendDataRequest();                                  //Send data request to all nodes.
  }
  lora();                                               //Handles LoRa traffic.
}

void mesureDistance() {
  distanceSensor.startRanging();                        //Write configuration bytes to initiate measurement.
  while (!distanceSensor.checkForDataReady()) delay(1); //waits until data is ready for reading.
  float distance = distanceSensor.getDistance();        //Get the result of the measurement (in mm) from the sensor.
  distanceSensor.clearInterrupt();                      //Clear the interrupt flag. (interrupt is currently unused)
  distanceSensor.stopRanging();                         //Stops taking measurements.
  saveToSD(0, distance / 10, InternalBatt());           //Saves (current time; distance in cm; battery state) to the file named ID_0.csv on the SD card.
}

bool InternalBatt() {
  //TODO
  return 0;         //tells the battery is OK, eventhoug we dont have a battery currently.
}

void sendDataRequest() {
  uint8_t duta[] = "B";           //Makes an array consisting of B and a null termination.
  rf95.send(duta, sizeof(duta));  //Sends the B to all LoRa devices.
  rf95.waitPacketSent();          //Waits until any previous packet is finished transmitting.
}

//Documentation: http://www.airspayce.com/mikem/arduino/RadioHead/classRH__RF95.html
void lora() { 
  if (rf95.available())           //Checks if valid uncorrupted data is ready for retrival from the module.
  {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN]; //Create an array that can hold the biggest possible recived message.
    uint8_t len = sizeof(buf);            //save the length of the array, as a fallback.
    if (rf95.recv(buf, &len))             //This funcion copys the data string form the module to the buf array, -
    {                                     //it also overwrites len to the correct length.
      digitalWrite(led, HIGH);            //We have now recived somthing, to show our happiness we blind our enemies with a bright blue light.
      if (len == 4) {                     //if the length is 4, we asume its one of our nodes.
        float i = ((buf[1] << 8) + buf[2]);   //byte 1 and 2 contains our 16bit data. we move the bits to the correct positions with bitshift, and pluses them to be one 16b byte
        bool bat = 0;                     //bat siginfies low battery, its set to 0 = OK.
        i = (i == 65526) ? -1 : i / 10;   //if the sensor is blocked we recive 65526 we make it into -1. if not blocked we devide the data with 10 to get cm.
        Serial.printf("Got %3.1fcm. from device: %d. RSSI: %d", i, buf[0], rf95.lastRssi());    //for debug purposes this prints all the data to the serial monitor.
        if (buf[3] == 'q') {              //if we have a low battery warning we recive 'q'.
          bat = 1;                        //bat is set to 1 = low battery.
          Serial.print(" low Battery");   //for debug we print low battery
        }else{
          //Here other buf[3] commands can be checked on. if empty, the compiler ignores it.
        }
        Serial.println("");               //prints a newline character to the serial port.
        saveToSD(buf[0], i, bat);         //We save the data to the SD card in a csv file.
        uint8_t data[] = "A";             //we send a confirmation character to the node. (For debug purposes, not checked on by nodes).
        rf95.send(data, sizeof(data));    //Sends the A to all LoRa devices.
        rf95.waitPacketSent();            //Waits until any previous packet is finished transmitting.
        digitalWrite(led, LOW);           //turns off the LED.
      }
    }
    else
    {
      Serial.println("recv failed");
    }
  }
}

void saveToSD(uint8_t Addr, float Data, bool LowBat) {
  char path[15];                                          //make an array big enough to the path.
  sprintf(path, "/ID_%u.CSV", Addr);                      //create the path string which includes the device ID, and saves into path array.
  char text[20];                                          //make an array big enough to hold the string we want to save to the SD card.
  char B = LowBat ? 'Y' : 'N';                            //make B either Y or N, depending on low battery warning.
  sprintf(text, "\n%lu;%3.1f;%c", millis(), Data, B);     //Create string ex. ("60000;25.2;Y";) and save it into text[]
  if (testFileName(SD, path)) {                           //check if the file exist on the SD card. if not we want to create it.
    writeFile(SD, path, "Tid;afstand (cm);Low Battery?"); //Create a file and write the into the file. (the string is the CSV header).
  }
  appendFile(SD, path, text);                             //Append the text[] string to the path[] on the SD card. 
}

void writeFile(fs::FS &fs, const char * path, const char * message) { //borrowed from SD example.
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);                  //writes the path to the SD card
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {                              //Writes the CSV header to the newly created file.
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

bool testFileName(fs::FS &fs, const char * dirname) { //In this funcion we want to test if a file exist.
  File root = fs.open("/");                 //We only operate in root. so we want to open root.
  if (!root) {                              //If we couldnt open root somting went wrong.
    Serial.println("Failed to open directory");
    return 1;                               //and we do a tactical retreat.
  }
  if (!root.isDirectory()) {                //Make sure Root is a directory.
    Serial.println("Not a directory");
    return 1;
  }

  File file = root.openNextFile();          //Opens the first file.
  while (file) {                            //Loops as long as we have untested files.
    if (!file.isDirectory()) {              //Check to see if its a file or a folder.
      if (!strcmp(file.name(), dirname)) {  //Compare the filename to the currently opened file.
        return 0;                           //if the file exists we want to return 0 = Exist.
      }
    }
    file = root.openNextFile();             //Opens the next file.
  }
  return 1;                                 //File did not exist.
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);     //open path
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (!file.print(message)) {                 //Append message to file.
    Serial.println("Append failed");
  }
  file.close();
}
