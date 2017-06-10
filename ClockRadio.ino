#include "EEPROMAnything.h"
//#include <Adafruit_ESP8266.h>
#include "Radio.h"
#include "SparkFunSi4703.h"
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
#include "EEPROMAnything.h"

#define CURRENT_VERSION 3


#define DISPLAY_ADDRESS 0x70
#define CAP_ADDRESS 0x5A
#define SEVENSEG_DIGITS 4

Ticker ticker;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
Adafruit_7segment clockDisplay = Adafruit_7segment();
Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_TPA2016 audioamp = Adafruit_TPA2016();
Si4703_Breakout radio(2, 4, 5);

uint16_t lasttouched = 0;
uint16_t currtouched = 0;

int lastHourTimeChecked = 0;
int display_indicators = 0x02;
bool displayUpdated = false;
bool displayNeedsUpdated = false;

int time_minutes = 0;
int time_hours = 0;
int time_seconds = 0;

bool alarm1_state = false;
bool alarm2_state = false;

struct context_t
{
  int snooze_time = -1;
  bool alarm_tripped = false;

} context;

struct configuration_t
{
  byte initialized;
  int version;
  int display_brightness;
  int amp_volume;
  int radio_volume;
  int radio_channel;
  int alarm1_hours;
  int alarm1_minutes;
  int alarm2_hours;
  int alarm2_minutes;
  int alarm1_on;
  int alarm2_on;
  int time_offset;
} config;
bool config_changed = 0;

void setup()
{
  //initalize config;
  Serial.println("Starting Up");

  WiFiManager wifiManager;
  wifiManager.autoConnect("PenguinClock");
  Serial.println("Wifi Connected.");
  radio.powerOn();
  Serial.println("Radio Powered On");
  delay(100);

  scani2c();
  
  bool capon = false;
  capon = cap.begin(CAP_ADDRESS);
  Serial.print("Cap Sensor: ");
  Serial.println(capon);

  clockDisplay.begin(DISPLAY_ADDRESS);
  timeClient.begin();
  
  config_load();
  
  ticker.attach(1, tick);

  updateTime(true);
  updateDisplay();
  radio_setup();
  Serial.println("Radio Setup Complete");
  amp_setup();
  Serial.println("Amp Setup Complete");
  
}

void amp_TurnOn()
{
  Serial.println("Amp turned on");
  audioamp.enableChannel(true, true);
}

void amp_TurnOff()
{
  Serial.println("Amp turned off");
  audioamp.enableChannel(false, false);
}


void config_load()
{
  Serial.println("Loading EEPROM settings.");
  EEPROM.begin(4096);
  EEPROM_readAnything(0, config);
  if (config.initialized == 0 || config.version != CURRENT_VERSION)
  {
    Serial.println("EEPROM settings not initializing, setting default values.");
    config.initialized = 1;
    config.version = CURRENT_VERSION;
    config.display_brightness = 2;
    config.alarm1_hours = 5;
    config.alarm1_hours = 5;
    config.alarm1_minutes = 30;
    config.alarm2_hours = 7;
    config.alarm2_minutes = 0;
    config.radio_channel = 961;
    config.radio_volume = 7;
    config.amp_volume = 6;
    config.time_offset = 4;
    config_save();
  }

  //Testing
  config.alarm1_hours = 15;
  config.alarm1_minutes = 32;

  // /Testing
  displayInfo();
}

void config_save()
{
  Serial.println("Saving EEPROM settings.");
  EEPROM_writeAnything(0, config);
  EEPROM.commit();
  config_changed = false;
}

void radio_setup()
{
  //radio.powerOn();
  radio.setVolume(config.radio_volume);
  radio.setChannel(config.radio_channel);
  displayInfo();
}

void amp_setup()
{
  audioamp.begin();
  Serial.println("Amp Begin Complete");
  delay(50);

  audioamp.setGain(config.amp_volume);
  Serial.println("Amp Set Gain Complete");
  //audioamp.enableChannel(true, true);
  //amp_TurnOn();
  amp_TurnOff();
}

void displayInfo()
{
  Serial.printf("Channel: %d Volume: %d Amp: %d", config.radio_channel, config.radio_volume, config.amp_volume);
  Serial.printf("\tAlarm1: %2d:%2d Alarm2: %2d:%2d", config.alarm1_hours, config.alarm1_minutes, config.alarm2_hours, config.alarm2_minutes);
  Serial.printf("\tTimeOffset: %d", config.time_offset);
  Serial.printf("\tDisplayBrightness: %2d\n", config.display_brightness);
}

void updateTime(bool force)
{
  if (!force && lastHourTimeChecked == time_hours)
  {
    return;
  }

  lastHourTimeChecked = time_hours;
  timeClient.setTimeOffset(-config.time_offset * 3600);
  timeClient.update();
  
  time_seconds = timeClient.getSeconds(); 
  time_hours = timeClient.getHours();
  time_minutes = timeClient.getMinutes();
  Serial.printf("Updated to: %d:%2d:%2d\n", time_hours, time_minutes, time_seconds);
}

void tick(void) 
{
  time_seconds++;
  if (time_seconds > 59) 
  {
    displayNeedsUpdated = true;
    time_seconds = 0;
    time_minutes += 1;
    if (time_minutes > 59) 
    {
      time_minutes = 0;
      time_hours += 1;
      if (time_hours > 23) 
      {
        time_hours = 0;
      }
    }
    updateDisplay();
  }

  //check Alarms
  {
    if (alarm1_state == false 
      && config.alarm1_hours == time_hours 
      && config.alarm1_minutes == time_minutes)
    {
      amp_TurnOn();
      alarm1_state = true;
    }

    if (alarm2_state == false
      && config.alarm2_hours == time_hours
      && config.alarm2_minutes == time_minutes)
    {
      amp_TurnOn();
      alarm2_state = true;
    }
  }

  if (config_changed)
  {
    config_save();
  }
  Serial.printf("tick %d:%d:%d\n", time_hours, time_minutes, time_seconds);
}

void updateDisplay()
{
  displayNeedsUpdated = false;
  int indicators = indicators | 0x02; //make sure colon is on.

  int displayValue = time_hours * 100 + time_minutes;
  if (time_hours > 12)
  {
    displayValue -= 1200;
  }
  // Handle hour 0 (midnight) being shown as 12.
  else if (time_hours == 0)
  {
    displayValue += 1200;
  }
  
  if (time_hours > 11)
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
  clockDisplay.setBrightness(config.display_brightness);
  Serial.printf("Display: %3d\t%2d\n", displayValue, config.display_brightness);
  
}

void loop()
{
  //get the time from NTP every hour
  if (time_minutes == 0)
  {
    updateTime(false);
  }

  //update the display when needed;
  if (displayNeedsUpdated)
  {
    updateDisplay();
  }

  checkButtons();

}

#define BUTTON_SNOOZE 0
#define BUTTON_RESET 1
#define BUTTON_RADIO 2
#define MODE_ALARMONOFF 3
#define MODE_RADIOTUNING 4
#define MODE_ALARM2SET 5
#define MODE_ALARM1SET 6
#define MODE_TIME 7
#define MODE_VOLUME 8
#define MODE_BRIGHTNESS 9
#define BUTTON_PLUS 10
#define BUTTON_MINUS 11

void checkButtons()
{
  currtouched = cap.touched();
  
  // Brightness
  if (currtouched & _BV(MODE_BRIGHTNESS))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setBrightness(1);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setBrightness(1);
    }
  }
  
  // Volume
  if (currtouched & _BV(MODE_VOLUME))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setVolume(1);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setVolume(-1);
    }
  }

  // Time
  if (currtouched & _BV(MODE_TIME))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setOffset(1);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setOffset(-1);
    }
  }

  // Tuning
  if (currtouched & _BV(MODE_RADIOTUNING))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setTuning(+2);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setTuning(-2);
    }
  }


  // Alarm1
  if (currtouched & _BV(MODE_ALARM1SET))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setAlarm(1, 1);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setAlarm(1, -1);
    }
  }


  // Alarm2
  if (currtouched & _BV(MODE_ALARM2SET))
  {
    if ((currtouched & _BV(BUTTON_PLUS)) && !(lasttouched & _BV(BUTTON_PLUS)))
    {
      setAlarm(2, 1);
    }
    if ((currtouched & _BV(BUTTON_MINUS)) && !(lasttouched & _BV(BUTTON_MINUS)))
    {
      setAlarm(2, -1);
    }
  }

  lasttouched = currtouched;
}



void setBrightness(int amount)
{
  config.display_brightness + amount;
  if (config.display_brightness > 15) { config.display_brightness = 15; }
  if (config.display_brightness < 0) { config.display_brightness = 0; }
  clockDisplay.setBrightness(config.display_brightness);
  
  Serial.printf("Brightness: %d\n", config.display_brightness);
  displayNeedsUpdated = true;
  config_changed = true;
}

void setOffset(int amount)
{
  config.time_offset++;
  if (config.time_offset > 14) { config.time_offset = 14; }
  if (config.time_offset < -14) { config.time_offset = -14; }
  updateTime(true);
  displayNeedsUpdated = true;
  Serial.printf("Offset: %d\n", config.time_offset);
  config_changed = true;
}

void setVolume(int amount)
{
  config.amp_volume + amount;
  if (config.amp_volume > 30) { config.amp_volume = 30; }
  if (config.amp_volume < 0) { config.amp_volume = 0; }
  
  Serial.printf("AmpVolume: %d\n", config.amp_volume);
  audioamp.setGain(config.amp_volume);
  config_changed = true;
}

void setTuning(int amount)
{
  config.radio_channel += amount;
  if (config.radio_channel > 1081) { config.radio_channel = 851; }
  if (config.radio_channel < 851) { config.radio_channel = 1081; }
  
  Serial.printf("Channel: %d\n", config.radio_channel);
  radio.setChannel(config.radio_channel);
  config_changed = true;
}


void setAlarm(int alarm, int amount)
{
  if (alarm == 1)
  {
    config.alarm1_minutes + amount;
    if (config.alarm1_minutes > 59)
    {
      config.alarm1_hours++;
      config.alarm1_minutes = 0;
    }

    if (config.alarm1_minutes < 0)
    {
      config.alarm1_hours--;
      config.alarm1_minutes = 59;
    }
    Serial.printf("Alarm1: %d:%d\n", config.alarm1_hours, config.alarm1_minutes);
  }
  else if (alarm == 2)
  {
    config.alarm2_minutes + amount;
    if (config.alarm2_minutes > 59)
    {
      config.alarm2_hours++;
      config.alarm2_minutes = 0;
    }

    if (config.alarm2_minutes < 0)
    {
      config.alarm2_hours--;
      config.alarm2_minutes = 59;
    }
    Serial.printf("Alarm2: %d:%d\n", config.alarm2_hours, config.alarm2_minutes);
  }

  config_changed = true;
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