#include <Adafruit_CircuitPlayground.h>
#include <TEA5767.h>
#include <SI4705.h>
#include <SI4703.h>
#include <RDSParser.h>
#include <RDA5807M.h>
#include <radio.h>
#include <newchip.h>
#include <Adafruit_TPA2016.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_LEDBackpack.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

#define DISPLAY_ADDRESS 0x70
#define CAP_ADDRESS 0x5A
#define SEVENSEG_DIGITS 4

Ticker ticker;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -4 * 60 * 60, 60000);
Adafruit_7segment clockDisplay = Adafruit_7segment();
Adafruit_MPR121 cap = Adafruit_MPR121();

Adafruit_TPA2016 audioamp = Adafruit_TPA2016();

bool toggle;
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

int indicators = 0x02;

int minutes = 0;
int hours = 0;
int seconds = 0;
unsigned long count;
unsigned long next;
bool updated = false;
void setup()
{
  WiFiManager wifiManager;
  wifiManager.autoConnect("PenguinClock");
  Serial.println("Wifi Connected.");

  Serial.println("Starting up...");
  //pinMode(0, OUTPUT);

  bool capon = false;
  capon = cap.begin(CAP_ADDRESS);
  Serial.print("Cap Sensor: ");
  Serial.println(capon);

  clockDisplay.begin(DISPLAY_ADDRESS);
  timeClient.begin();
  updateTime();
  
  //testdisplay();
  //testamp();
  //scani2c();

  updateDisplay();

  ticker.attach(1, tick);

}

void updateTime()
{
  timeClient.update();
  seconds = timeClient.getSeconds(); 
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  Serial.printf("Updated to: %d:%d:%d\n", hours, minutes, seconds);
}

void tick(void) 
{
  seconds++;
  if (seconds > 59) {
    seconds = 0;
    minutes += 1;
    if (minutes > 59) 
    {
      minutes = 0;
      hours += 1;
      if (hours > 23) 
      {
        hours = 0;
      }
    }
  }
  updated = false;
  Serial.printf("tick %d:%d:%d\n", hours, minutes, seconds);
}

void updateDisplay()
{
  int indicators = indicators | 0x02; //make sure colon is on.

  int displayValue = hours * 100 + minutes;
  if (hours > 12)
  {
    displayValue -= 1200;
  }
  // Handle hour 0 (midnight) being shown as 12.
  else if (hours == 0)
  {
    displayValue += 1200;
  }
  
  if (hours > 11)
  {
    indicators = indicators | 0x04; //turn on PM indicator
  }
  else 
  {
    indicators = indicators & B11111011; //turn off PM indicator
  }

  clockDisplay.print(displayValue, DEC); 
  clockDisplay.writeDigitRaw(2, indicators);
  clockDisplay.writeDisplay();

  Serial.printf("Display: %3d\n", displayValue);
  updated = true;
}

void loop()
{
  //get the time from NTP every hour
  if (minutes == 0)
  {
    updateTime();
  }
  
  //update the display every minute;
  if (seconds == 0 && !updated)
  {
    updateDisplay();
  }

  currtouched = cap.touched();

  for (uint8_t i = 0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" touched");
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i))) {
      Serial.print(i); Serial.println(" released");
    }
  }

}








void scani2c()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++)
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address<16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

  //delay(5000);           // wait 5 seconds for next scan
}



void testamp()
{

  audioamp.begin();

  // Dump register map, for debugging purposes.
  //for (uint8_t i=1; i<8; i++) {
  //Serial.print("Register #"); Serial.print(i);
  //Serial.print(": 0x");
  //Serial.println(audioamp.read8(i), HEX);
  //}

  // Turn off AGC for the gain test
  audioamp.setAGCCompression(TPA2016_AGC_OFF);
  // we also have to turn off the release to really turn off AGC
  audioamp.setReleaseControl(0);
  audioamp.setGain(15);

  // We can update the gain, from -28dB up to 30dB
  /*for (int8_t i = -28; i <= 30; i++) {
  Serial.print("Gain = "); Serial.println(i);
  audioamp.setGain(i);
  delay(500);
  }*/

  // Each channel can be individually controlled
  Serial.println("Left off");
  audioamp.enableChannel(true, false);
  delay(1000);
  Serial.println("Left On, Right off");
  audioamp.enableChannel(false, true);
  delay(1000);
  Serial.println("Left On, Right On");
  audioamp.enableChannel(true, true);
  delay(1000);
  Serial.println("Left Off, Right Off");
  audioamp.enableChannel(false, false);
  delay(1000);

  // OK now we'll turn the AGC back on and mess with the settings :)

  // AGC can be TPA2016_AGC_OFF (no AGC) or
  //  TPA2016_AGC_2 (1:2 compression)
  //  TPA2016_AGC_4 (1:4 compression)
  //  TPA2016_AGC_8 (1:8 compression)
  //Serial.println("Setting AGC Compression");
  //audioamp.setAGCCompression(TPA2016_AGC_2);

  // See Datasheet page 22 for value -> dBV conversion table
  //Serial.println("Setting Limit Level");
  //audioamp.setLimitLevelOn();
  // or turn off with setLimitLevelOff()
  //audioamp.setLimitLevel(25);  // range from 0 (-6.5dBv) to 31 (9dBV)

  // See Datasheet page 23 for value -> ms conversion table
  //Serial.println("Setting AGC Attack");
  //audioamp.setAttackControl(5);

  // See Datasheet page 24 for value -> ms conversion table
  //Serial.println("Setting AGC Hold");
  //audioamp.setHoldControl(0);

  // See Datasheet page 24 for value -> ms conversion table
  //Serial.println("Setting AGC Release");
  //audioamp.setReleaseControl(11);
}
void testdisplay()
{



  //clockDisplay.drawColon(true);
  //clockDisplay.writeDisplay();

  clockDisplay.writeDigitRaw(2, 0x02);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x04);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x08);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x10);
  clockDisplay.writeDisplay();
  delay(1500);



  clockDisplay.writeDigitRaw(2, 0x06);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x0c);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x18);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitRaw(2, 0x1e);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.print(0, DEC);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.print(10, DEC);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.writeDigitNum(0, 8);
  clockDisplay.writeDisplay();
  delay(1500);
  clockDisplay.writeDigitNum(1, 8);
  clockDisplay.writeDisplay();
  delay(1500);
  clockDisplay.writeDigitNum(3, 8);
  clockDisplay.writeDisplay();
  delay(1500);
  clockDisplay.writeDigitNum(4, 8);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.print(1000, DEC);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.print(3000, DEC);
  clockDisplay.writeDisplay();
  delay(1500);

  clockDisplay.setBrightness(0);
  delay(1500);
  clockDisplay.setBrightness(3);
  delay(1500);
  clockDisplay.setBrightness(6);
  delay(1500);
  clockDisplay.setBrightness(9);
  delay(1500);
  clockDisplay.setBrightness(12);
  delay(1500);
  clockDisplay.setBrightness(15);
  delay(1500);

}