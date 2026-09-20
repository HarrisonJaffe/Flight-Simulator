[README.md](https://github.com/user-attachments/files/32440306/README.md)
# Flight-Simulator Motion Platform

A 4-actuator motion platform that reads pitch and roll data from FlightGear in real time and physically tilts a seat/rig to match, using an Arduino Mega for closed-loop actuator control.

## How it works

```
FlightGear  --telnet-->  Python (PC)  --serial-->  Arduino Mega  --PWM-->  4x Linear Actuators
                                                         ^
                                                         |
                                                    MPU6050 IMU (feedback)
```

1. **FlightGear** is run with its telnet interface enabled, exposing live flight properties (altitude, pitch, roll).
2. On the PC, a Python script connects over telnet, reads pitch/roll, and streams it to the Arduino over serial as a simple `pitch,roll\n` string, at roughly 10 Hz.
3. The **Arduino Mega** receives the target pitch/roll, compares it against the platform's *actual* current orientation (measured with an onboard MPU6050 IMU), and drives four linear actuators — one at each corner (front-left, front-right, back-left, back-right) — to move the platform toward the target angle.
4. Each actuator has extend/retract limit switches so the firmware never drives it past its physical travel.
5. If the PC stops sending data (e.g. FlightGear closes or the serial link drops) for more than 3 seconds, the Arduino automatically levels the platform as a fail-safe.

## Hardware

- Arduino Mega 2560
- 4x linear actuators (one per corner of the platform)
- 4x dual-channel motor driver modules (RPWM/LPWM/R_EN/L_EN control, e.g. BTS7960-style)
- 8x limit switches (extended + retracted per actuator, for travel limiting)
- MPU6050 IMU (orientation feedback for the platform itself)
- PC running FlightGear

## Repo contents

| File | Description |
|---|---|
| `FlightGearCode.py` | Connects to FlightGear over telnet and reads live altitude, pitch, and roll. |
| `FlightGearHandler.py` | Main PC-side loop: pulls pitch/roll from `FlightGearCode.py`, opens a serial connection to the Arduino, streams motion data continuously, and handles reconnect/retry logic. |
| `SendDataToArduino.py` | Small standalone script for testing the serial link to the Arduino independent of FlightGear. |
| `ArduinoFlightSimV2.ino` | Arduino Mega firmware. Reads IMU orientation, computes per-actuator commands from the target pitch/roll, drives the actuators, respects limit switches, and handles startup tensioning and fail-safe leveling. |
| `Videos/` | Demo footage of the platform in action. |

## Setup

1. **FlightGear**: launch with the telnet interface enabled, e.g.
   ```
   fgfs.exe --telnet=socket,bi,60,localhost,5500,tcp
   ```
2. **Arduino**: open `ArduinoFlightSimV2.ino` in the Arduino IDE (requires the `Adafruit_MPU6050` and `Adafruit_Sensor` libraries), select the Mega 2560 board, and upload it.
3. **PC**: install the Python dependencies (`flightgear_python`, `pyserial`), update the serial port in `FlightGearHandler.py` (defaults to `COM5`) to match your Arduino, and run:
   ```
   python FlightGearHandler.py
   ```
4. On startup, the platform will tension itself against its limit switches and level before it starts responding to FlightGear.

## Status

This was a personal project built a few years ago — the code here is preserved as-is. Wiring diagrams, CAD, and other build details beyond what's in this repo are no longer available.
