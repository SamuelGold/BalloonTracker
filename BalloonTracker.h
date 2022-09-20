#define CALLSIGN "N0CALL"

#define DEVMODE 1

/***********************************************************************************
* DEFAULT FSK SETTINGS, thanks to https://github.com/RoelKroes/TBTracker/
*  
* Normally needs no change
************************************************************************************/
#define FSK_FREQUENCY 437.600
#define FSK_BITRATE 100.0
#define FSK_FREQDEV 50.0
#define FSK_RXBANDWIDTH 125.0
#define FSK_POWER 20   // in dBm between 2 and 17. 10 = 10mW (recommended). Sets also RTTY power
#define FSK_PREAMBLELENGTH 16
#define FSK_ENABLEOOK false
#define FSK_DATASHAPING 0.5

/***********************************************************************************
* RTTY SETTINGS
*  
* Change when needed
* Default RTTY setting is: 7,N,2 at 100 Baud.
************************************************************************************/
#define RTTY_ENABLED true            // Set to true if you want RTTY transmissions (You can use Both LoRa and RTTY or only one of the two) 
#define RTTY_PAYLOAD_ID  "RTTY_ID"   // Payload ID for RTTY protocol
#define RTTY_FREQUENCY  437.600      // Can be different from LoRa frequency
#define RTTY_SHIFT 425
#define RTTY_BAUD 100                // Baud rate
#define RTTY_STOPBITS 1
#define RTTY_PREFIX "$$"
#define RTTY_IDLE_TIME 1000          // Idle carrier in ms before sending actual RTTY string.

// RTTY encoding modes (leave this unchanged)
#define RTTY_ASCII 0                 // 7 data bits 
#define RTTY_ASCII_EXTENDED 1        // 8 data bits
#define RTTY_ITA2  2                 // Baudot 

// the current state of balloon
struct ball_info
{
  double latitude; // coordinates
  double longitude;
  long gps_altitude; // altitude according to GPS
  
  long gps_time; // 180435
  char gps_time_str[10]; // 18:04:35
  
  long gps_date; // 311222
  long altitude; // altitude according to baro sensor
  long pressure; // atmospheric pressure
  float temperature_outside; // temperature outside the box
  float temperature_inside; // temperature inside the box
} ball_info = {0};
