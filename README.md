# Smart AirGuard - IoT Monitoring System

<div align="center">

![Smart AirGuard System](https://img.shields.io/badge/Platform-ESP32-blue)
![MQTT-Adafruit IO](https://img.shields.io/badge/MQTT-Adafruit_IO-orange)
![Node-RED](https://img.shields.io/badge/Node--RED-Automation-red)
![Telegram Bot](https://img.shields.io/badge/Telegram-Bot-blue)
![ThingSpeak](https://img.shields.io/badge/Cloud-ThingSpeak-orange)

*An IoT-based gas monitoring and automated ventilation system for garages and workshops*

</div>

## Table of Contents
- [Overview](#overview)
- [Key Features](#key-features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Pin Assignment](#pin-assignment)
- [Software Dependencies](#software-dependencies)
- [Telegram Bot Commands](#-telegram-bot-commands)
- [Cloud Platforms and Data Management](#cloud-platforms-and-data-management)
  - [Adafruit IO (MQTT Broker)](#-adafruit-io-integration)
  - [ThingSpeak (Data Logging)](#-thingspeak-integration)
  - [Node-RED (Orchestration & ML)](#node-red-integration-and-research-layer)
  - [Telegram Bot (User Interface)](#node-red-integration-and-research-layer)
- [Alert System](#-alert-system)
- Testing & Validation
- Cost Estimation
- Wiring Diagram

## Overview

Smart AirGuard is an IoT-based environmental monitoring and safety system designed specifically for enclosed automotive environments such as garages and workshops. The system continuously monitors air quality (CO₂-equivalent), temperature, humidity, and occupancy using low-cost sensors, providing real-time hazard detection and automated mitigation.

Unlike traditional gas detectors, Smart AirGuard implements a hybrid communication architecture that separates safety-critical functions (running on the ESP32) from advanced analytics and experimentation (running on Node-RED). This ensures that even during network outages, the system autonomously activates ventilation and audible alarms when dangerous gas levels are detected.

The project was developed as a Master's thesis at the Polytechnic Institute of Beja, achieving 0% false positives and 0% false negatives across all experimental tests.

![Project](prototype.png)

## Key Features

### 📊 **Multi-Sensor Monitoring**
- **Air Quality**: MQ-135 gas sensor for detecting harmful gases (10–5000 ppm CO₂-equivalent)
- **Temperature & Humidity**: DHT22 sensor for climate monitoring (-40°C to 80°C, 0–100% RH)
- **Occupancy Detection**: HC-SR501 PIR sensor (3–7m range)
- **Visual Indicators**: RGB LED for motion, status LEDs for alerts

### 🔔 **Smart Alert System**
- **Instant Telegram Notifications** for:
  - Dangerous gas levels 
  - Temperature extremes (<10°C or >35°C)
  - High humidity (>90%)
  - Motion detection
- **Audible Alarms**: Active buzzer for critical gas levels (>1000 ppm)
- **Visual indicators**: RGB LED (motion) + discrete LEDs (gas, temperature, humidity)

### 🌬️ **Automated Ventilation**
- **Relay-controlled fan** activates within **<1 second** of gas threshold exceedance
- Manual override via Node-RED dashboard or Telegram commands
- Automatic return to AUTO mode after 5 minutes of inactivity

### 🧠 **Advanced Analytics (Node-RED)**
- **Machine Learning predictions**: 15-minute and 30-minute gas forecasts
- **Risk assessment** (0–100%) with preventive fan activation at >70% risk
- **Model Validation** framework with MAE, RMSE, MAPE, and Accuracy metrics
- **Ten simulated hazard scenarios** for systematic testing

### 🌐 **Cloud Integration**
- **ThingSpeak Cloud**: Real-time data logging every 15 seconds
- **Telegram Bot**: Two-way communication with the system
- **Local Display**: 0.96 inch OLED for on-device monitoring

### ☁️ **Multi-Platform Cloud Architecture**
- **Adafruit IO**: MQTT broker for real-time messaging
- **ThingSpeak**: Long-term data logging with MATLAB analytics
- **Node-RED**: Orchestration, ML, testing, and dashboard
- **Telegram Bot**: User notifications and bidirectional control

### 💰 **Low Cost**
- **Total hardware cost**: ~31.01 EUR per unit
- **Free cloud tiers** for academic prototyping
- **Commercial deployment options** from 85–210 EUR/year

## System Architecture

System architecture is shown on Figure below. 

![System_architecture](drawio.png)

## Hardware Requirements

### **Main Components**
| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32 DOIT DevKit V1 | 1 | Main microcontroller (dual-core, WiFi, Bluetooth) |
| DHT22 | 1 | Temperature (-40 to 80°C) & humidity (0–100% RH) |
| MQ-135 | 1 | Gas sensor (10–5000 ppm CO₂-equivalent) |
| HC-SR501 PIR | 1 | Motion detection (3–7m range) |
| 0.96" OLED (SSD1306) | 1 | Local display (128×64, I²C) |
| RGB LED (Common Cathode) | 1 | Motion status indication (blinking blue) |
| 5V 2-Channel Relay Module | 1 | Fan control (active LOW configuration) |
| DC 5V Cooling Fan | 1 | Ventilation actuator |
| Active Buzzer | 1 | Audible emergency alerts |
| LED (Red) | 1 | Gas danger indicator (>1000 ppm) |
| LED (Yellow) | 1 | Temperature alert (<10°C or >35°C) |
| LED (Green) | 1 | Humidity alert (>90%) |
| Power Bank / 5V USB | 1 | Power supply |

### **Power Requirements**
- **Input**: 5V DC via USB or external power supply
- **Current**: ~120mA (normal), ~160mA (Wi-Fi TX), ~145mA (relay active)
- **Total cost**: 31.01 EUR (all components)

## Pin Assignment

### **ESP32 Pin Configuration**

| ESP32 Pin | Component | Type | Function |
|-----------|-----------|--------|-------|
| GPIO 14 | DHT22 | Digital I/O | Temperature & Humidity |
| GPIO 34 | MQ-135 | Analog input | Gas concentration (12-bit ADC, 0–4095) |
| GPIO 32 | HC-SR501 PIR | Digital input | Motion detection (HIGH when active) |
| GPIO 23 | OLED (SDA) | I2C | I2C Data line |
| GPIO 22 | OLED (SCL) | I2C | I2C Clock line |
| GPIO 33 | RGB LED (Blue) | Digital Output | Motion status (blinking when detected) |
| GPIO 13 | Active Buzzer | Digital Output | Emergency sound (1 kHz tone) |
| GPIO 21 | Red LED | Digital Output | Gas danger (>1000 ppm) |
| GPIO 19 | Yellow LED | Digital Output | Temperature alert (<10°C or >35°C) |
| GPIO 18 | Green LED | Digital Output | Humidity alert (>90%) |
| GPIO 26 | Relay Module | Digital Output | Fan control (active LOW: LOW=ON) |

![Scheme](AirGuard_scheme.png)

**Note:**

Relay module is active LOW: fan ON when pin = LOW, OFF when pin = HIGH (ensures fan remains off during ESP32 boot)
RGB LED (red/green channels) not used in current firmware
All LEDs use 220Ω current-limiting resistors
DHT22 data line uses 10kΩ pull-up to 3.3V
OLED I²C uses 4.7kΩ pull-ups, address 0x3C

### **Real physical board**
![Physics](physics.png)

### **Power Connections**
- **3.3V**: DHT22, OLED, PIR sensor
- **5V**: MQ-135, Buzzer, LEDs, Relay module
- **GND**: All components

## Software Dependencies

### **Arduino Libraries Required**
```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <MQ135.h>
```

### **Library Installation**
1. Open Arduino IDE
2. Go to **Tools → Manage Libraries**
3. Search and install:
```
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    adafruit/Adafruit GFX Library@^1.11.5
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/DHT sensor library@^1.4.4
    adafruit/Adafruit MQTT Library@^2.0.3
    arduino-libraries/MQ135@^1.0.0
```
**Important**: The firmware does NOT include UniversalTelegramBot.h. All Telegram communication is handled indirectly via Node-RED and MQTT.

## 🤖 Telegram Bot Commands

The Telegram bot is bridged through Node-RED (ESP32 never directly calls Telegram API). Supported commands:

| Command | Description | Example Response |
|---------|-------------|------------------|
| `/start` or `/help` | Show available commands | List of all commands with descriptions |
| `/status` | Full system status | `Gas: 450ppm (NORMAL), Temp: 22.5°C, Humidity: 55%, Motion: NO, Fan: AUTO` |
| `/sensors` | Real-time sensor data | `Temp: 22.5°C, Humidity: 55%, Gas: 450ppm, Motion: NO` |
| `/alerts` | Active hazard conditions | `GAS: NORMAL, TEMP: NORMAL, HUMIDITY: NORMAL` |
| `/fan on` | Manual fan activation | `Fan turned ON manually` |
| `/fan off` | Manual fan deactivation | `Fan turned OFF manually` |
| `/fan auto` | Return to automatic mode | `Fan switched to AUTO mode` |

### **TelegramBot layout**
![TelegramBot](TelegramBot.png)

### **Automatic Alerts**
The system automatically sends alerts for:
- 🚨 **Gas**: Level > 400 ppm
- 🌡️ **Temperature**: Outside 10-35°C range
- 💧 **Humidity**: Above 90%
- 🚶 **Motion**: When detected

## Cloud Platforms and Data Management

Smart AirGuard employs a **multi-platform cloud architecture** to balance real-time system responsiveness with long-term environmental data analysis. 

The system integrates **Adafruit IO**, **ThingSpeak**, and **Node-RED**, each addressing different functional and research requirements.

### 📊 ThingSpeak Integration

ThingSpeak is employed as the **data logging and analytical platform**. Sensor data is uploaded at fixed intervals and stored for long-term evaluation.

Its role within Smart AirGuard includes:

- Persistent storage of environmental sensor data

- Time-series visualization over extended periods

- Statistical analysis of gas concentration (mean, minimum, maximum)

- Weekly trend analysis and detection of missing or anomalous data

ThingSpeak supports MATLAB-based analytics, making it appropriate for **research-oriented post-processing**, while its higher latency limits its use in real-time control scenarios.

### **Data Fields Mapping**
| Field | Data | Type | Description |
|-------|------|------|-------------|
| 1 | Temperature | Float | Current temperature in °C |
| 2 | Humidity | Float | Current humidity in % |
| 3 | Gas Level | Integer | MQ-135 sensor reading |
| 4 | Motion | Binary | 1 = Detected, 0 = No motion |
| 5 | Gas Alert | Binary | 1 = Alert, 0 = Normal |
| 6 | Temp Alert | Binary | 1 = Alert, 0 = Normal |
| 7 | Humidity Alert | Binary | 1 = Alert, 0 = Normal |
| 8 | Fan Status | Binary | 1 = ON, 0 = OFF |

#### **Dashboard Configuration**
Widgets created in ThingSpeak to visualize:
- Temperature & Humidity graphs
- Gas level history
- Motion detection timeline
- Alert status indicators
- Weekly Gas Level Analytics

![ThingSpeak](ThingSpeak.png)

#### Weekly Gas Level Analytics
This module analyzes gas concentration data collected by the Smart AirGuard system and stored on ThingSpeak.

What It Does:

- Retrieves gas level and alert data for the last 7 days
- Handles UTC → local (Portugal) timezone conversion
- Processes data on a daily basis

![Analytics](Analytics1.png)
  
- Key Metrics

- Daily mean, maximum, and minimum gas levels

- Alert count per day

- Detection of days with missing data

📉 Visualizations

- Daily gas level time-series comparison

- Bar charts of daily statistics

- Alert frequency per day

- Distribution plots with safety thresholds (warning & danger levels)

## 🤖 Adafruit IO Integration

Adafruit IO is used as the **primary real-time communication layer** of the Smart AirGuard system. It provides low-latency, MQTT-based data exchange between the ESP32 device and external services.

In this project, Adafruit IO is responsible for:

- Real-time streaming of sensor data (temperature, humidity, gas concentration, motion)

- Bidirectional communication for actuator control (fan ON/OFF)

- Instant synchronization between the ESP32 and external dashboards

- Event-driven data delivery suitable for automation workflows

Due to its low latency and native MQTT support, Adafruit IO is well suited for **interactive monitoring and immediate response**, but it is not optimized for long-term analytical processing.

## Comparative Use of Adafruit IO and ThingSpeak

The simultaneous use of Adafruit IO and ThingSpeak is a deliberate architectural decision, as the platforms serve complementary purposes.

| Aspect | Adafruit IO | ThingSpeak |
|------|------------|------------|
| Primary Function | Real-time messaging and control | Data logging and analytics |
| Communication Model | MQTT (event-based) | HTTP (periodic uploads) |
| Latency | Low | High |
| Actuator Control | Supported | Not suitable |
| Long-term Analysis | Limited | Extensive |

By combining both platforms, Smart AirGuard achieves **fast safety responses** while preserving **rich historical datasets** for analytical evaluation.

## Node-RED Integration and Research Layer

Node-RED is used as an **integration, automation, and experimentation layer** within the Smart AirGuard architecture. It subscribes to real-time MQTT data streams from Adafruit IO and enables flexible data processing without modifying the ESP32 firmware.

Node-RED enables:

- Data aggregation and preprocessing

- Custom dashboards for visualization and manual control

- Rule-based automation and conditional logic

- Rapid prototyping of alternative alert and control strategies

Importantly, Node-RED is treated as a **research environment**, allowing experimental logic (e.g., threshold tuning, filtering techniques, sensor correlation analysis) to be evaluated independently of the embedded system.

The Node-RED flow implements:

- MQTT integration with Adafruit IO for sensor data

- Data normalization and routing to dashboard elements

![Smartairguarddatatab](Smartairguarddatatab.png)

![Nodeinfo](nodeinfo.png)

- Predictive module with buffer and forecasting logic

![PredictivemoduleMLtab](PredictivemoduleMLtab.png)

![Nodeml](nodeml.png)

- Remote fan control

![Fancontroltab](Fancontroltab.png)

![Nodefancontrol](nodefancontrol.png)

- Test mode controller for scenario-based validation

![Systemtestingandvalidationtab](Systemtestingandvalidationtab.png)

![Nodetestpanel](nodetestpanel.png)

- LED status monitoring for gas, temperature, and humidity alerts

![Nodegauges](nodegauges.png)

![Nodegraphs](nodegraphs.png)

## 🚨 Alert System

### **Priority Levels**
1. **CRITICAL** (Red LED + Buzzer + Telegram + Fan)
   - Gas level > 400 ppm
   - Immediate fan activation

2. **WARNING** (Yellow LED + Telegram)
   - Temperature outside range
   
3. **NOTICE** (Green LED + Telegram)
   - High humidity detected
   
4. **INFO** (RGB LED (Blue) + Telegram)
   - Motion detected/stopped

### **Alert Cooldown Mechanism**
- Each alert type has a 30-second cooldown
- Prevents notification spam
- Separate timers for gas, temperature, humidity
- Motion alerts have no cooldown for immediate response

## 🖥️ Local Display Information

The OLED display shows:
- Current sensor readings
- Alert status indicators
- Motion detection status
- Blue LED state (motion)
- Fan status
