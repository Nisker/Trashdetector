
#include <SPI.h>
#include <RH_RF95.h>

RH_RF95 rf95;

int led = 9;

void setup()
{
  pinMode(led, OUTPUT);
  Serial.begin(115200);
  while (!Serial) ; // Wait for serial port to be available
  if (!rf95.init())
    Serial.println("init failed");
  rf95.setTxPower(20, false);
}
unsigned long last = 0;
void loop()
{
  if (millis() - last > 4000) {
    last = millis();
    uint8_t duta[] = "B";
    rf95.send(duta, sizeof(duta));
    rf95.waitPacketSent();
  }

  if (rf95.available())
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len))
    {
      digitalWrite(led, HIGH);
      float i = ((buf[1] << 8) + buf[2]);
      i = i / 10;
      i > 500 ? Serial.printf("Blocked sensor. from device: %d. RSSI: %d\n", buf[0], rf95.lastRssi()) : Serial.printf("Got %3.1fcm. from device: %d. RSSI: %d\n", i, buf[0], rf95.lastRssi());

      // Send a reply
      uint8_t data[] = "A";
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      digitalWrite(led, LOW);
    }
    else
    {
      Serial.println("recv failed");
    }
  }
}
