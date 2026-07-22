import serial
import time

string = "abs"

# Configure the serial connection
arduino = serial.Serial(port='COM5', baudrate=9600, timeout=1)
time.sleep(2)  # Wait for Arduino to initialize

arduino.write(str(abs).encode('utf-8'))
time.sleep(0.01)

while arduino.in_waiting <= 0:
    time.sleep(0.01)
    response = arduino.readline().decode('utf-8').rstrip()
    print(response)
