
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <RH_RF95.h>          //http://www.airspayce.com/mikem/arduino/RadioHead/index.html
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Wire.h>
#include "DS3231.h"
#include "SparkFun_VL53L1X.h" //Click here to get the library: http://librarymanager/All#SparkFun_VL53L1X
#define PollingIntervalSeconds 5

const char* ssid = "Bo";
const char* password = "12345678";
const char* host = "trash";

SFEVL53L1X distanceSensor;//Defaults to (I2C @21+22).
RH_RF95 rf95;             //This defaults to use pins (CS = SPI_SS, Interupt pin = 2, SPI interface = VSPI).
WebServer server(80);
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
  }else hasSD = true;

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
  sprintf(text, "\n%lu;%3.1f;%c", RTC.now().unixtime(), Data, B);     //Create string ex. ("60000;25.2;Y";) and save it into text[]
  if (!SD.exists(path)) {                           //check if the file exist on the SD card. if not we want to create it.
    appendFile(SD, path, "Tid;afstand (cm);Low Battery?"); //Create a file and write the into the file. (the string is the CSV header).
  }
  appendFile(SD, path, text);                             //Append the text[] string to the path[] on the SD card. 
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

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.html";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".html")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.html";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (SD.exists((char *)upload.filename.c_str())) {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: "); Serial.println(upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
  }
}

void deleteRecursive(String path) {
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String output;
    if (cnt > 0) {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound() {
  if (hasSD && loadFromSdCard(server.uri())) {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void Wifiinit(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds
    delay(500);
  }
  if (i == 21) {
    Serial.print("Could not connect to ");
    Serial.println(ssid);
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

    if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    Serial.print("You can now connect to http://");
    Serial.print(host);
    Serial.println(".local");
  }

  
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, []() {
    returnOK();
  }, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}


void setup() {
  pinMode(led, OUTPUT);   //Sets LED to output.
  Wire.begin();           //initializes I2C.
  Serial.begin(115200);   //Initializes UART @ 115200 baud.
  delay(100);             //Waits to make sure UART is setup when we need it.

  Wifiinit();
  
  SDinit();               //Initializes SD card

  if (!rf95.init()) Serial.println("init failed"); //Initializes LoRa moudule and warns us if init faliled.
  rf95.setTxPower(20, false);                      //20dbm (100mW) Configuration. theoretical 2x distance in comparison to 14dbm (25mW)

  VL53init();             //Initializes ToF distance sensor.

}

void loop() {
  if (!(RTC.now().unixtime()%PollingIntervalSeconds)) {  //wait specified time in seconds.
    if (!WasHereOnce){
      mesureDistance();                                   //Locally mesures distance with ToF sensor.
      sendDataRequest();                                  //Send data request to all nodes.
      WasHereOnce = 1;
    }
  } else WasHereOnce = 0;
  lora();                                               //Handles LoRa traffic.
  server.handleClient();
}
