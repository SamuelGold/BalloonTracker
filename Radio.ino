/* Uncomment //#define RADIOLIB_STATIC_ONLY in BuildOpt.h to avoid RadioLib crash */

#define E220_30

#include "Arduino.h"
#include "LoRa_E220.h"
#include <RadioLib.h>

SPIClass *sx1278_SPI = new SPIClass(HSPI);

// SX1278 pins and SPI           NSS DIO0 RESET DIO1  SPI
SX1278 sx1278_radio = new Module(25, 32,  33,   27,  *sx1278_SPI);

const char e220_m0 = 18;
const char e220_m1 = 5;

// create RTTY client instance using the radio module
RTTYClient rtty(&sx1278_radio);

void SetupRTTY()
{

  // First setup FSK
  SetupFSK();
      
  Serial.print(F("RTTY init.."));
  int16_t state = rtty.begin(RTTY_FREQUENCY,
                             RTTY_SHIFT,
                             RTTY_BAUD,
                             RTTY_ASCII,
                             RTTY_STOPBITS);
                     
  if(state == RADIOLIB_ERR_NONE)
  {    
    Serial.println(F("done"));
  } else 
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
}

void SetupFSK()
{
  // Initialize the SX1278
  Serial.print(F("[SX1278] init.."));

  sx1278_SPI->begin();

  int16_t state = sx1278_radio.beginFSK(FSK_FREQUENCY,
                                        FSK_BITRATE,
                                        FSK_FREQDEV,
                                        FSK_RXBANDWIDTH,
                                        FSK_POWER,
                                        FSK_PREAMBLELENGTH,
                                        FSK_ENABLEOOK);
 
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed 0, code "));
    Serial.println(state);
  }
}

void rtty_message_send(char *message)
{
  rtty.idle();       
  delay(RTTY_IDLE_TIME);
   
  int state = rtty.println(message);
  
  delay(RTTY_IDLE_TIME / 5);
  rtty.standby();
}

char Hex(char Character)
{
  char HexTable[] = "0123456789ABCDEF";
  
  return HexTable[Character];
}

/* LoRa configuration using UART2 TX only, RX is occupated by GPS */
void lora_setup()
{ 
  byte E220_init[] = {0xc0, 0x00, 0x08, // command to write 8 bytes
                      0x00, 0x00, 0x62, 0x00, // 00:00 addr, serial port 9600 8N1, and air data rate 2.4K
                        24, 0x03, 0x00, 0x00}; // 24 channel (434.125MHz)

  pinMode(e220_m0, OUTPUT);
  pinMode(e220_m1, OUTPUT);

  // m0, m1 high- Configuration Mode
  digitalWrite(e220_m0, HIGH);
  digitalWrite(e220_m1, HIGH);
  delay(500);
      
  // several trials to configure
  for (int e220_inits = 0; e220_inits < 5; e220_inits++)
  {
      Serial2.write(E220_init, sizeof(E220_init));
      delay(500);
  }  

  // Normal mode
  digitalWrite(e220_m0, LOW);
  digitalWrite(e220_m1, LOW);
  delay(500);
}

void lora_message_send(char *message)
{  
  Serial2.write(message);
}
