#include <LowPower.h>
#include <LM35.h>
#include <afstandssensor.h>
#define TemperaturePin A0

//radiohead lora
#include <SPI.h>
#include <RH_RF95.h>
RH_RF95 rf95;

//definerer afstandsensor pins
AfstandsSensor afstandssensor(4, 5);

#define minspaending 5
LM35 temp(A1);

void setup()
{
  Serial.begin(9600);
  while (!Serial) ;
  if (!rf95.init())
    Serial.println("init failed");

  //sætter signal styrke db.
  rf95.setTxPower(20, false);
  
  pinMode(A2, INPUT);
  pinMode(A1, INPUT);

  analogReference(INTERNAL);
}

void lorasend (double i) {
  Serial.println("Sending to rf95_server");
  // Send a message to rf95_server
  int s = (int) i;
  uint8_t data [3];
  data[1] = (s >> 8) & 0xff;
  data[2] = (s) & 0xff;
  data[0] = 69;
  rf95.send(data, sizeof(data));

  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.waitAvailableTimeout(3000))
  {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len))
    {
      Serial.print("got reply: ");
      Serial.println((char*)buf);
      //      Serial.print("RSSI: ");
      //      Serial.println(rf95.lastRssi(), DEC);
    }
    else
    {
      Serial.println("recv failed");
    }
  }
  else
  {
    Serial.println("No reply, is rf95_server running?");
  }
  delay(4000);
}

void wakeUp() {
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (rf95.recv(buf, &len)) {
    if (!strcmp(buf, "B")) {
      double i = afstandssensor.afstandCM();
      Serial.print("afstand: ");
      Serial.println(i);
      lorasend(i * 10);
    }
  }
}

//reads voltage on battery, and return 1 if voltage is under 1.9.
bool spaending() {
  float adc = analogRead(A2);
  float factor = (463 / 51.2) * 1.22;
  float spaending = (adc / 1024) * factor;

  Serial.print("Spænding: ");
  Serial.print(spaending);

  Serial.print(" adc: ");
  Serial.print(adc);

  delay(1000);

  if (spaending <= minspaending) return 1;
  return 0;
}

void loop() {
  if (rf95.available()) wakeUp();
  Serial.print("temp: ");
  Serial.print(temp.cel());
  Serial.println(" ");
  spaending();

  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
