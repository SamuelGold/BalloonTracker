"""
Weather baloon ground station
"""

from machine import UART,Pin
import random
import utime
import st7789
import tft_config
import vga1_8x16 as font
import vga1_16x32 as font_big
import binascii
from SIM800L import Modem
import json
import hashlib

tft = tft_config.config(3)

e220_m0 = Pin(14, Pin.OUT)
e220_m1 = Pin(15, Pin.OUT)
e220_aux = Pin(16, Pin.IN, Pin.PULL_UP)
e220_tx = Pin(13)
e220_rx = Pin(12)
e220_uart = UART(0, 9600, tx = e220_rx, rx = e220_tx)

e220_uart.init(9600, bits = 8, parity = None, stop = 1, timeout = 100)

gsm = Modem(MODEM_TX_PIN = 9, MODEM_RX_PIN = 8)

def e220_init():
    e220_m0.value(1)
    e220_m1.value(1)
    utime.sleep_ms(1)
    
    while ( not e220_aux.value() ):
        utime.sleep_ms(1)
        
    print("E220 AUX waiting done! Writing configuration...")   
#############################################################
    #set RX channel, 0x18 = 24 dec = 434.125MHz
    e220_uart.write(binascii.unhexlify("c0040118"))
    utime.sleep_ms(1)
    while ( (not e220_aux.value()) or e220_uart.read() ):
        print("Wait after frequency change...")
        utime.sleep_ms(100)

    if e220_uart.any():
        print("c0040117 setting result:", binascii.hexlify(e220_uart.readline()))
#############################################################
    #set broadcast RX
    e220_uart.write(binascii.unhexlify("c000020000"))
    #e220_uart.write(binascii.unhexlify("c00003111162"))
    utime.sleep_ms(1)
    while ( (not e220_aux.value()) or e220_uart.read() ):
        print("Wait after address change...")
        utime.sleep_ms(100)

    utime.sleep_ms(10)
    
    if e220_uart.any():
        print("c000020000 setting result:", binascii.hexlify(e220_uart.readline()))
#############################################################    
    e220_uart.write(binascii.unhexlify("c10008"))
    utime.sleep_ms(250)
    if e220_uart.any():
        print("Configuration:", binascii.hexlify(e220_uart.readline()))
#############################################################
    e220_m0.value(0)
    e220_m1.value(0)
    utime.sleep_ms(10)

    while ( (not e220_aux.value()) or e220_uart.read() ):
        print("Wait for the mode change")
        utime.sleep_ms(100)
    
    print("E220 initialzation finished ")

def display_gsm_signal(percent = 50):
    text = str(percent)
    tft.text(font, '█' * 3, 207, 0, st7789.BLACK, st7789.BLACK)
    tft.text(font, text, 207, 0, st7789.YELLOW, st7789.BLACK)

def display_gsm_bat(voltage = 4.0):
    voltage = round(voltage, 1)
    text = str(voltage)
    tft.text(font, '█' * 4, 206, 14, st7789.BLACK, st7789.BLACK)
    tft.text(font, text, 206, 14, st7789.YELLOW, st7789.BLACK)
    tft.text(font, 'V', 232, 14, st7789.RED, st7789.BLACK)
    
def print_line(line, x, y):
    length = len(line)
    tft.text(font_big, line, x - (length * 16), y - 26, st7789.WHITE, st7789.BLACK)
    
def clear_line(length, x, y):
    #tft.text(font_big, "X" * length, x - (length * 16), y - 26, st7789.RED, st7789.BLACK)
    tft.text(font_big, " " * length, x - (length * 16), y - 26, st7789.BLACK, st7789.BLACK)

#lat_lon
def display_lat_lon(lat_n, lon_e):
    clear_line(8, 147, 60)
    print_line(str(lat_n), 147, 60)
    clear_line(8, 147, 95)
    print_line(str(lon_e), 147, 95)
    
# hight
def display_height(m, m_baro):
    clear_line(5, 80, 129)
    print_line(str(m), 80, 129)
    clear_line(5, 185, 129)
    print_line(str(m_baro), 185, 129)

# hPa
def display_hpa(hpa):
    clear_line(4, 168, 27)
    print_line(str(hpa), 168, 27)

# seconds
def display_seconds(seconds):
    clear_line(4, 88, 27)
    print_line(str(seconds), 88, 27)

# *C
def display_temperature(c, c2):
    clear_line(3, 213, 60)
    print_line(str(c), 213, 60)
    clear_line(3, 213, 95)
    print_line(str(c2), 213, 95)

def verify_message(message):
    #b'$$NOCALL,3630,17:46:50,53.13479,23.17336,151,78,31.1,21.8,1000*74EB\n'
    info_array = []
    
    try:
        message_decoded = message.decode('ascii')
        print('Got message: ', message_decoded)
            
        splited_message = str(message_decoded).split(',')
                
        element = splited_message[1] # Packet number
        if (not element.isdigit()):
            print('packet number error:', element)
            return None
        info_array.append(element)

        element = splited_message[2] # Time
        if (len(element) != 8):
            print('Time decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[3] # Lat
        try:
            float(element)
        except ValueError:
            print('Lat decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[4] # Lon
        try:
            float(element)
        except ValueError:
            print('Lon decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[5] # Alt GPS
        if (not element.isdigit()):
            print('Alt GPS decoding error:', element)
            return None
        info_array.append(element)
        
        element = splited_message[6] # Alt Baro
        if (not element.isdigit()):
            print('Alt baro decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[7] # Temp int
        try:
            float(element)
        except ValueError:
            print('Temp int decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[8] # Temp out
        try:
            float(element)
        except ValueError:
            print('Temp Out decoding error:', element)
            return None
        info_array.append(element)

        element = splited_message[9].split('*')[0] # Baro
        if (not element.isdigit()):
            print('Baro decoding error:', element)
            return None
        info_array.append(element)

        return info_array
            
    except:
        return None
    
    return None

def log_file(message):
    f = open("fly_journal.txt")
    f.write("213\n")
    f.close()

def SaveLogFile(filename, dictionary):
    with open(filename, "a") as f:
        for key in range(0, len(dictionary)):
            if key != len(dictionary) - 1:
                f.write(str(dictionary[key]) + " ")
            else:
                f.write(str(dictionary[key]) + "\n")

tft.init()
tft.jpg(f'bg.jpg', 0, 0, st7789.SLOW)
gsm.initialize()
e220_init()

#gsm.connect(apn='internet', user='internet', pwd='internet')
#print('\nModem IP address: "{}"'.format(gsm.get_ip_addr()))
#print('\nNow running demo http GET...')
#url = 'http://checkip.dyn.com/'
#response = gsm.http_request(url, 'GET')
#print('Response status code:', response.status_code)
#print('Response content:', response.content)
display_gsm_signal(0)

#utime.sleep(1000)

e220_m0.value(0)
e220_m1.value(0)
utime.sleep_ms(100)

#s = '$$NOCALL,6374,16:18:44,53.13480,23.17329,130,91,28.2,19.2,999*EBDD\n'.split(',')

#status = {'Lat': str(s[3]),
#          'Lon': str(s[4]),
#          'Altitude_GPS': str(s[5]),
#          'ALtitude_Baro': str(s[6]),
#          'Temperature_in': int(float(str(s[7]))),
#          'Temperature_out': int(float(str(s[8]))),
#          'Pressure': str(s[9]).split('*')[0]}


#            _sentence_b64 = binascii.b2a_base64(message, newline=False).decode('ascii')
#            print("base64:", _sentence_b64)

#            _data = {
#                "type": "payload_telemetry",
#                "data": {
#                    "_raw": _sentence_b64
#                },
#                "receivers": {
#                    "N0CALL": {"time_created": "2022-06-19T17:12:13.174562Z", "time_uploaded": "2022-06-19T17:12:13.174562Z",},
#                },
#            }

#            json_data_str = str(_data)

#            print("Data:", json_data_str)

#            doc_id_hash = hashlib.sha256()
#            doc_id_hash.update(_sentence_b64)
#            doc_id_str = binascii.hexlify(doc_id_hash.digest()).decode('ascii')
#            print("DOC_ID:", doc_id_str)







#print(status)

#print("GSM signal info:", int(gsm.get_signal_strength() * 100))

n = 1
seconds = 0

while n != 0:
    #LoRa got: b'$$NOCALL,6618,11:31:42,0.00000,0.00000,0,136,29.0,27.8,994*3229\n'
    if e220_uart.any():
        message = verify_message(e220_uart.readline())
        if (message != None):
            display_hpa(message[8])
            display_height(message[4], message[5])
            display_lat_lon(message[2], message[3])
            display_temperature(int(float(str(message[7]))), int(float(str(message[6]))))
            SaveLogFile("/FlyLog.txt", message)
            seconds = 0

    display_seconds(seconds)
    bat_status = gsm.battery_status().split(',')    
    display_gsm_bat(float(bat_status[2])/1000)

    utime.sleep_ms(1000)
    seconds = seconds + 1
