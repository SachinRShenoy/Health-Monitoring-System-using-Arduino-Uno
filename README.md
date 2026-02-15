# Health Monitoring System using Arduino Uno

## Description

A microcontroller-based health monitoring system using Arduino Uno. It measures heart rate using KY-039 and body temperature using DHT11, displays readings on an I2C 16x2 LCD, streams data via Bluetooth (HC-05), and sends SMS alerts via SIM900A GSM module upon user confirmation. Blood pressure monitoring is omitted due to unavailability and high cost of IoT-compatible sensors.

## Features

* Measures body temperature (°C) using DHT11 sensor.
* Measures heart rate (BPM) using KY-039 sensor.
* Displays readings on 16×2 I2C LCD.
* Streams data to smartphone via HC-05 Bluetooth.
* Sends SMS alerts via SIM900A GSM module when 'OK' is typed.
* Tested each component individually before integration.

## Hardware Components

| Component    | Description                   | Quantity | Cost (INR) |
| ------------ | ----------------------------- | -------- | ---------- |
| Arduino Uno  | Microcontroller board         | 1        | 330        |
| KY-039       | Heart rate sensor             | 1        | 40         |
| DHT11        | Temperature sensor            | 1        | 49         |
| LCD I2C 16×2 | Display module                | 1        | 0          |
| HC-05        | Bluetooth module              | 1        | 230        |
| GSM SIM900A  | GSM module with 12V 1A supply | 1        | 945 + 150  |
| Breadboard   | Solderless breadboard         | 1        | 55         |
| Jumper Wires | Male-Male / Breadboard wires  | 1 set    | 40         |

**Total cost:** 1,839 INR

## System Architecture

* Arduino Uno is the main controller.
* DHT11 connected to digital pin 2.
* KY-039 connected to analog pin A0.
* LCD I2C on SDA/SCL (A4/A5).
* HC-05 via SoftwareSerial (pins 10/11).
* SIM900A via SoftwareSerial (pins 7/8), powered with 12V 1A supply.
* All grounds are common.

**Schematic Diagram:** `schematic_connection.png`

## Usage Instructions

1. Connect hardware as per schematic.
2. Insert SIM card into GSM module.
3. Power GSM module with 12V 1A supply.
4. Upload `Health_Monitoring.ino` to Arduino Uno via Arduino IDE.
5. Open Serial Monitor at 9600 baud.
6. Monitor live temperature and BPM on LCD.
7. To send SMS, type **OK** in Serial Monitor or Bluetooth terminal.

## Notes

* Blood pressure monitoring is not included.
* GSM requires valid SIM and network.
* Adjust `FINGER_THRESHOLD` in code based on KY-039 readings.
* Bluetooth data can be accessed via any serial terminal app.

## Folder Structure

```
Health-Monitoring-System-Arduino/
├── Health_Monitoring.ino       # Arduino sketch
├── schematic_connection.png    # Wiring diagram
├── README.md                   # This file
└── components_photos/          # Optional: add component images
```

## Future Scope

* Integrate blood pressure monitoring when budget allows.
* Log readings to SD card or cloud.
* Develop smartphone app for better visualization and alerts.
