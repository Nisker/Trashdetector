#include <afstandssensor.h>

//radiohead lora
#include <SPI.h>
#include <RH_RF95.h>
RH_RF95 rf95;

//definerer afstandsensor pins
AfstandsSensor afstandssensor(3, 4);

void setup() 
{
  Serial.begin(9600);
  while (!Serial) ;
  if (!rf95.init())
    Serial.println("init failed");

  //sætter signal styrke db.
  rf95.setTxPower(20, false);

}

void lorasend (double i){
  Serial.println("Sending to rf95_server");
  // Send a message to rf95_server
  int s = (int) i;
uint8_t data [3];
  data[1] = (s>>8)&0xff;
  data[2] = (s)&0xff;
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


void loop(){

double i = afstandssensor.afstandCM();

Serial.print("afstand: ");
Serial.println(i);

lorasend(i*10);

  
}
