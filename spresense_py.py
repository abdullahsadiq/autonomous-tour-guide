import RPi.GPIO as GPIO
import serial
import time

ser = serial.Serial('/dev/ttyUSB0', 115200)
oePin = 17

class SpresenseSerial:
    def update(self):
        print('Starting Spresense serial connection...')
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(oePin,GPIO.OUT)
    def run_threaded(self):
        if(ser.in_waiting >0):
            line = ser.readline()
            command = int(line)
            if(command == 1):
                GPIO.output(oePin,GPIO.HIGH)
                print('Stopping DonkeyCar')
            if(command == 0):
                GPIO.output(oePin,GPIO.LOW)
                print('DonkeyCar free to move')