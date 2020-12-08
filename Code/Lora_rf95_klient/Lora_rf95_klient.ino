#include <LowPower.h>
#include <afstandssensor.h>

//radiohead lora
#include <SPI.h>
#include <RH_RF95.h>
RH_RF95 rf95;

//definerer afstandsensor pins
AfstandsSensor afstandssensor(4, 5);

#define minspaending 5


void setup()
{
  Serial.begin(115200);
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
  uint8_t data [4];
  data[1] = (s >> 8) & 0xff;
  data[2] = (s) & 0xff;
  data[0] = 69;
  //if voltage is under 5v, spaending returns 1, and data[3] sends a q to master, which means low voltage.
  //otherwise it sends 0 which means OK.
  data[3] = spaending() ? 'q' : '0';
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
}

void wakeUp() {
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (rf95.recv(buf, &len)) {
    if (!strcmp(buf, "B")) {
      float temp = analogRead(A0);
      temp = (temp * 0.48828125) / 4.225306122;
      Serial.print(" temp: ");
      Serial.print(temp);
      double i = afstandssensor.afstandCM(temp);
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
  spaending();

  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
