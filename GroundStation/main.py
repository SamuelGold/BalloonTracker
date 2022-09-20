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

tft = tft_config.config(3)

e220_m0 = Pin(14, Pin.OUT)
e220_m1 = Pin(15, Pin.OUT)
e220_aux = Pin(16, Pin.IN, Pin.PULL_UP)
e220_tx = Pin(13)
e220_rx = Pin(12)
e220_uart = UART(0, 9600, tx = e220_rx, rx = e220_tx)

e220_uart.init(9600, bits=8, parity=None, stop=1, timeout = 100)

gsm = Modem(MODEM_TX_PIN = 9,
            MODEM_RX_PIN = 8)

#gsm_tx = Pin(8)
#gsm_rx = Pin(9)
#gsm_uart = UART(1, 115200, tx = gsm_tx, rx = gsm_rx)

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
    utime.sleep_ms(1000)
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
    
def clear_line(line, x, y):
    length = len(line)
    tft.text(font_big, line, x - (length * 16), y - 26, st7789.RED, st7789.BLACK)

#lat_lon
def disaplay_lat_lon(lat_n, lon_e):
    clear_line('█' * 8, 147, 60)
    print_line(str(lat_n), 147, 60)
    clear_line('█' * 8, 147, 95)
    print_line(str(lon_e), 147, 95)
    
# hight
def disaplay_height(m, m_baro):
    clear_line('█' * 5, 80, 129)
    print_line(str(m), 80, 129)
    clear_line('█' * 5, 185, 129)
    print_line(str(m_baro), 185, 129)

# hPa
def disaplay_hpa(hpa):
    clear_line('█' * 4, 168, 27)
    print_line(str(hpa), 168, 27)

# seconds
def disaplay_seconds(seconds):
    clear_line('.' * 5, 88, 27)
    #print_line(str(seconds), 88, 27)

# *C
def disaplay_temperature(c, c2):
    clear_line('█' * 3, 213, 60)
    print_line(str(c), 213, 60)
    clear_line('█' * 3, 213, 95)
    print_line(str(c2), 213, 95)

#def display_info():

tft.init()
tft.jpg(f'bg.jpg', 0, 0, st7789.SLOW)
#hello()

gsm.initialize()
e220_init()

#utime.sleep(1000)

e220_m0.value(0)
e220_m1.value(0)
utime.sleep_ms(100)

s = '$$N0CALL,6374,16:18:44,53.00000,23.00000,130,91,28.2,19.2,999*EBDD\n'.split(',')

status = {'Lat': str(s[3]),
          'Lon': str(s[4]),
          'Altitude_GPS': str(s[5]),
          'ALtitude_Baro': str(s[6]),
          'Temperature_in': int(float(str(s[7]))),
          'Temperature_out': int(float(str(s[8]))),
          'Pressure': str(s[9]).split('*')[0]}

print(status)

print("GSM signal info:", int(gsm.get_signal_strength() * 100))

n = 1
seconds = 0
while n != 0:
    #LoRa got: b'$$N0CALL,6618,11:31:42,0.00000,0.00000,0,136,29.0,27.8,994*3229\n'
    if e220_uart.any():
        message = e220_uart.readline()
        if (message != None):
            print("LoRa got:", message)
            s = str(message).split(',')
            disaplay_hpa(str(s[9]).split('*')[0])
            disaplay_height(str(s[5]), str(s[6]))
            disaplay_lat_lon(s[3], s[4])
            disaplay_temperature(int(float(str(s[8]))), int(float(str(s[7]))))
        
            seconds = 0
    
    display_gsm_signal(int(gsm.get_signal_strength() * 100))
    disaplay_seconds(seconds)
    
    bat_status = gsm.battery_status().split(',')    
    display_gsm_bat(float(bat_status[2])/1000)

    utime.sleep_ms(1000)
    seconds = seconds + 1



print("End")
