/* Uncomment //#define RADIOLIB_STATIC_ONLY in BuildOpt.h to avoid RadioLib crash */

#define E220_30
#define CALLSIGN "SP4SWD"

#include "Arduino.h"
#include "LoRa_E220.h"
#include <RadioLib.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

SPIClass *sx1278_SPI = new SPIClass(HSPI);

// SX1278 pins and SPI           NSS DIO0 RESET DIO1  SPI
SX1278 sx1278_radio = new Module(25, 32,  33,   27,  *sx1278_SPI);

// create AFSK client instance using the FSK module
// pin 26 is connected to SX1278 DIO2
AFSKClient audio(&sx1278_radio, 26);

// create AX.25 client instance using the AFSK instance
AX25Client ax25(&audio);

// create APRS client isntance using the AX.25 client
APRSClient aprs(&ax25);

// E220 UART and pins          AUX M0  M1
LoRa_E220 e220_radio(&Serial2, 4,  18, 5);

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

// the current state of balloon
struct ball_info
{
  double latitude; // coordinates
  double longitude;
  long gps_altitude; // altitude according to GPS
  long gps_time; // 180435
  long gps_date; // 311222
  float altitude; // altitude according to baro sensor
  float pressure; // atmospheric pressure
  float temperature_outside; // temperature outside the box
  float temperature_inside; // temperature inside the box
} ball_info = {0};

static bool barometer_found = false;
static bool ext_termo_found = false;

void aprs_setup()
{
  sx1278_SPI->begin();
  
  // initialize SX1278
  Serial.print(F("[SX1278] Initializing ... "));
  int state = sx1278_radio.beginFSK(432.5, 4.8, 5.0, 125.0, 20, 16, false);

  state = sx1278_radio.setPreambleLength(240);
  state = sx1278_radio.setFrequency(432.500);
  state = sx1278_radio.setBitRate(1200);
  state = sx1278_radio.setFrequencyDeviation(2.2);
  state = sx1278_radio.setOutputPower(20.0);
  
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed 0, code "));
    Serial.println(state);
  }

  // initialize AX.25 client
  Serial.print(F("[AX.25] Initializing ... "));
  
  // source station callsign:     CALLSIGN
  // source station SSID:         11
  // preamble length:             8 bytes
  state = ax25.begin(CALLSIGN, 11);
  
  if(state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("success!"));
  }
  else
  {
    Serial.print(F("failed 1, code "));
    Serial.println(state);
  }

  // initialize APRS client
  Serial.print(F("[APRS] Initializing ... "));
  
  // symbol: 'O' (BALLOON)
  state = aprs.begin('O');
  if(state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("success!"));
  }
  else
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
}

/* LoRa configuration using UART2 TX only, RX is occupated by GPS */
void lora_setup()
{ 
  // tartup all pins and UART
  e220_radio.begin();
  
  Configuration configuration = *(Configuration*) malloc(sizeof(Configuration));
  
  configuration.ADDL = 0x03;
  configuration.ADDH = 0x00;
  configuration.CHAN = 24;
  configuration.SPED.uartBaudRate = UART_BPS_9600;
  configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
  configuration.SPED.uartParity = MODE_00_8N1;
  configuration.OPTION.subPacketSetting = SPS_128_01;
  configuration.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED;
  configuration.OPTION.transmissionPower = POWER_27;
  configuration.TRANSMISSION_MODE.enableRSSI = RSSI_DISABLED;
  configuration.TRANSMISSION_MODE.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  configuration.TRANSMISSION_MODE.enableLBT = LBT_DISABLED;
  configuration.TRANSMISSION_MODE.WORPeriod = WOR_500_000;

  // several trials to configure
  for (int e220_inits = 0; e220_inits < 5; e220_inits++)
  {
    e220_radio.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    delay(100);
  }  
}

bool barometer_setup()
{
  unsigned status;

  status = barometer.begin(0x76);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    return false;
  }

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
  
  while(!Serial || !Serial2){};
  delay(500);
  
  ext_termo_found = ds18b20_setup();
  barometer_found = barometer_setup();

  aprs_setup();
  lora_setup();
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
    ball_info.pressure = barometer.readPressure();
    ball_info.altitude = barometer.readAltitude(1013.25);
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
  }

  if (gps.date.isValid())
  {
    ball_info.gps_date = gps.date.day() * 10000 + gps.date.month() * 100 + gps.date.year();
  } 

  if (gps.altitude.isValid())
  {
    ball_info.gps_altitude = gps.altitude.meters();
  }

  char lora_info[128];  
  snprintf(lora_info, sizeof(lora_info), "/"CALLSIGN"/%.6fN/%.6fE/%im/%.1fm/%.1fC/%.1fC/%.1fPa/%ld_%ld/",
          ball_info.latitude, ball_info.longitude, ball_info.gps_altitude, ball_info.altitude,
          ball_info.temperature_inside, ball_info.temperature_outside, ball_info.pressure, ball_info.gps_date, ball_info.gps_time);
          
  Serial.print("LoRa message: ");
  Serial.println(lora_info);
  
  e220_radio.sendMessage(lora_info);
  
  delay(2000);

  char aprs_lat[16], aprs_lon[16], aprs_time[16], aprs_msg[64];

  int lat_d = int(ball_info.latitude);
  int lat_m = int((ball_info.latitude - float(lat_d)) * 60 + 0.5);
  int lat_s = int((ball_info.latitude - float(lat_d) - float(lat_m)/60) * 3600 + 0.5);

  int lon_d = int(ball_info.longitude);
  int lon_m = int((ball_info.longitude - float(lon_d)) * 60 + 0.5);
  int lon_s = int((ball_info.longitude - float(lon_d) - float(lon_m)/60) * 3600 + 0.5);
  
  snprintf(aprs_lat, sizeof(aprs_lat), "%02i%02i.%02iN", lat_d, lat_m, lat_s);
  snprintf(aprs_lon, sizeof(aprs_lat), "%03i%02i.%02iE", lon_d, lon_m, lon_s);

  snprintf(aprs_time, sizeof(aprs_time), "%02i%04iz", ball_info.gps_date/10000, ball_info.gps_time / 100);
  snprintf(aprs_msg, sizeof(aprs_msg), "Alt %im(GPS) %.1fm(baro) Temp %.1fCint %.1fCext %.1fPa", ball_info.gps_altitude, ball_info.altitude, ball_info.temperature_inside, ball_info.temperature_outside, ball_info.pressure);
  
  aprs.sendPosition("WIDE1", 1, aprs_lat, aprs_lon, aprs_msg, aprs_time);

  if (ext_termo_found)
  {
    sensors.requestTemperatures();
  }
  
  delay(4000);
}
