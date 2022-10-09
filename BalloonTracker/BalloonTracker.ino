/* Uncomment //#define RADIOLIB_STATIC_ONLY in BuildOpt.h to avoid RadioLib crash */

#include "Arduino.h"
#include "BalloonTracker.h"
#include <TinyGPSPlus.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// GPS NMEA parser
TinyGPSPlus gps;

// BMP280 sensor  SDA SCL 21, 22
Adafruit_BMP280 barometer; // I2C

// setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(23); // oneWire pin 23

// pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

static bool barometer_found = false;
static bool ext_termo_found = false;
static int message_id = 0;

bool barometer_setup()
{
  unsigned status;

  status = barometer.begin(0x76);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    return false;
  }

  barometer.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                        Adafruit_BMP280::SAMPLING_X16,     /* Temp. oversampling */
                        Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                        Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                        Adafruit_BMP280::STANDBY_MS_125); /* Standby time. */

  return true;
}

bool ds18b20_setup()
{
  sensors.begin();
  delay(100);
  
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  if (!sensors.getAddress(insideThermometer, 0))
  {
    Serial.println("Unable to find address for temperature Device 0");
  }

  sensors.setResolution(insideThermometer, 10);

  return true;
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  EEPROM.begin(512);
  
  while(!Serial || !Serial2){};
  delay(500);
  
  ext_termo_found = ds18b20_setup();
  barometer_found = barometer_setup();

  SetupRTTY();
  lora_setup();

  //EEPROM.put(0, message_id); // uncomment at the first flashing
  EEPROM.get(0, message_id);
}

void loop() {
  // sensors processing
  if (ext_termo_found)
  {
    float tempC = sensors.getTempC(insideThermometer);
  
    // Check if reading was successful
    if(tempC != DEVICE_DISCONNECTED_C) 
    {
      ball_info.temperature_outside = tempC;
    }
    else
    {
      ball_info.temperature_outside = 0;
    }
  }
  
  if (barometer_found)    // Check if the measurement is complete
  {
    ball_info.temperature_inside = barometer.readTemperature();
    ball_info.pressure = barometer.readPressure()/100;
    ball_info.altitude = barometer.readAltitude(1010.30);
  }
  
  // GPS processing
  while (Serial2.available())
  {
    char c = Serial2.read();
    gps.encode(c);
  }

  if (gps.location.isValid())
  {
    ball_info.latitude = gps.location.lat();
    ball_info.longitude = gps.location.lng();
  }

  if (gps.time.isValid())
  {
    ball_info.gps_time = gps.time.hour() * 10000 + gps.time.minute() * 100 + gps.time.second();

    snprintf(ball_info.gps_time_str, sizeof(ball_info.gps_time_str), "%02d:%02d:%02d",
             gps.time.hour(), gps.time.minute(), gps.time.second());
  }
  else
  {
    snprintf(ball_info.gps_time_str, sizeof(ball_info.gps_time_str), "00:00:00");
  }
           
  if (gps.date.isValid())
  {
    ball_info.gps_date = gps.date.day() * 10000 + gps.date.month() * 100 + gps.date.year();
  } 

  if (gps.altitude.isValid())
  {
    ball_info.gps_altitude = gps.altitude.meters();
  }


  char message[128] = {0};

  snprintf(message, sizeof(message), "%s%s,%i,%s,%.5f,%.5f,%i,%i,%.1f,%.1f,%i",
           RTTY_PREFIX, CALLSIGN, message_id, ball_info.gps_time_str, 
           ball_info.latitude, ball_info.longitude, ball_info.gps_altitude, ball_info.altitude,
           ball_info.temperature_inside, ball_info.temperature_outside, ball_info.pressure);

  unsigned int Count = strlen(message);
  
  // Calc CRC
  unsigned int CRC = 0xffff;           // Seed
  unsigned int xPolynomial = 0x1021;
  
  for (int i = strlen(RTTY_PREFIX); i < Count; i++)
  {   // For speed, repeat calculation instead of looping for each bit
    CRC ^= (((unsigned int)message[i]) << 8);
    for (int j=0; j<8; j++)
    {
        if (CRC & 0x8000)
            CRC = (CRC << 1) ^ 0x1021;
        else
            CRC <<= 1;
    }
  }
  
  message[Count++] = '*';
  message[Count++] = Hex((CRC >> 12) & 15);
  message[Count++] = Hex((CRC >> 8) & 15);
  message[Count++] = Hex((CRC >> 4) & 15);
  message[Count++] = Hex(CRC & 15);
  message[Count++] = '\n';  
  message[Count++] = '\0';
    
  Serial.print("Message: ");
  Serial.println(message);


  EEPROM.put(0, message_id++);
  EEPROM.commit();

  lora_message_send(message);

  delay(1000);

  rtty_message_send(message);

  if (ext_termo_found)
  {
    sensors.requestTemperatures();
  }
  
  delay(10000);
}
