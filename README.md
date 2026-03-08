# Smart AirGuard - IoT Monitoring System

<div align="center">

![Smart AirGuard System](https://img.shields.io/badge/Platform-ESP32-blue)
![Telegram Bot](https://img.shields.io/badge/Telegram-Bot-blue)
![ThingSpeak](https://img.shields.io/badge/Cloud-ThingSpeak-orange)

*A comprehensive IoT-based air quality monitoring and alert system with real-time notifications*

</div>

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Software Dependencies](#software-dependencies)
- [Telegram Bot Commands](#-telegram-bot-commands)
- [Cloud Platforms and Data Management](#cloud-platforms-and-data-management)
  - [ThingSpeak Integration](#-thingspeak-integration)
  - [Adafruit IO Integration](#-adafruit-io-integration)
  - [Comparative Use of Adafruit IO and ThingSpeak](#comparative-use-of-adafruit-io-and-thingspeak)
  - [Node-RED Integration and Research Layer](#node-red-integration-and-research-layer)
- [Alert System](#-alert-system)
- [Local Display Information](#-local-display-information)

## Overview

Smart AirGuard is an IoT-based environmental monitoring and safety system designed for enclosed automotive and indoor environments. The system continuously monitors air quality, temperature, humidity, and motion using low-cost sensors, including an MQ-135 gas sensor, a DHT22 temperature and humidity sensor, and a PIR motion sensor. Local visual feedback is provided through RGB and status LEDs, while a compact OLED display presents real-time sensor readings directly on the device.

The system implements an intelligent alert and automation mechanism to ensure user safety. When predefined thresholds are exceeded — such as elevated gas concentration (>400 ppm), abnormal temperature ranges, high humidity, or detected motion—Smart AirGuard generates immediate notifications via a Telegram bot. In critical gas scenarios, the system additionally activates an audible buzzer and automatically controls a ventilation fan to mitigate hazardous conditions.

At the core of the system is an ESP32 microcontroller, responsible for real-time data acquisition, local decision-making, and wireless communication. For cloud integration, Smart AirGuard adopts a multi-platform approach. Adafruit IO is used as the primary real-time communication layer, enabling low-latency data streaming and remote actuator control. ThingSpeak is employed for periodic data logging, long-term storage, and statistical analysis of environmental parameters.

To support flexibility and experimentation, Node-RED is integrated as an intermediate automation and research layer. It enables data aggregation, custom visualization, and rapid prototyping of alternative alerting and control strategies without modifying the embedded firmware. This architecture allows Smart AirGuard to combine immediate safety responses with long-term data-driven analysis and experimental evaluation.

Overall, Smart AirGuard represents a scalable and modular IoT solution that integrates real-time monitoring, automated risk mitigation, cloud-based analytics, and a research-oriented software layer to enhance air quality monitoring and system extensibility.

![Project](project.png)

## Features

### 📊 **Multi-Sensor Monitoring**
- **Air Quality**: MQ-135 gas sensor for detecting harmful gases
- **Temperature & Humidity**: DHT22 sensor for climate monitoring
- **Motion Detection**: PIR sensor for presence detection
- **Visual Indicators**: RGB LED for motion, status LEDs for alerts

### 🔔 **Smart Alert System**
- **Instant Telegram Notifications** for:
  - Dangerous gas levels (>400 ppm)
  - Temperature extremes (<10°C or >35°C)
  - High humidity (>90%)
  - Motion detection
- **Audible Alarms**: Buzzer for critical gas levels
- **Automatic Ventilation**: Fan activation during high gas concentration

### 🌐 **Cloud Integration**
- **ThingSpeak Cloud**: Real-time data logging every 15 seconds
- **Telegram Bot**: Two-way communication with the system
- **Local Display**: 0.96 inch OLED for on-device monitoring

### 🎮 **Control & Interaction**
- Remote status checks via Telegram
- Real-time sensor data requests
- Alert configuration and monitoring
- System health monitoring

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Smart AirGuard System                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌──────────────┐    │
│  │ DHT22   │  │ MQ-135  │  │ PIR     │  │ OLED         │    │
│  │ Temp/Hum│  │ Gas     │  │ Motion  │  │ Display      │    │
│  └────┬────┘  └────┬────┘  └────┬────┘  └─────┬───────-┘    │
│       │            │            │             │             │
├───────┼────────────┼────────────┼─────────────┼───────────-─┤
│       │            │            │             │             │
│  ┌────▼────┐ ┌────-▼───┐ ┌────-─▼──┐    ┌───-─▼──────┐      │
│  │ ESP32   │ │ RGB LED │ │ Buzzer  │    │ Relay      │      │
│  │         │ │ Status  │ │ Alarm   │    │ Fan        │      │
│  │         │ │ Ind.    │ │         │    │ Control    │      │
│  └────┬────┘ └─────────┘ └─────────┘    └────┬─────-─┘      │
│       │                                      │              │
├───────┼──────────────────────────────────────┼──────────--──┤
│       │                                      │              │
│  ┌────▼──────┐                      ┌───────-▼────┐         │
│  │ WiFi      │                      │ External    │         │
│  │ Connection│                      │ 5V Fan      │         │
│  └────┬──────┘                      └─────────────┘         │
│       │                                                     │
├───────┼───────────────────────────────────────────────────--┤
│       │                                                     │
│  ┌────▼────────────┐           ┌──────────────────┐         │
│  │ Telegram Bot    │◄─────────►│ ThingSpeak Cloud │         │
│  │ Notifications   │           │ Data Logging     │         │
│  │ & Control       │           │ & Analytics      │         │
│  └─────────────────┘           └──────────────────┘         │
└─────────────────────────────────────────────────────────────┘
```

## Hardware Requirements

### **Main Components**
| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32 Dev Board | 1 | Main microcontroller |
| DHT22 Sensor | 1 | Temperature & humidity |
| MQ-135 Gas Sensor | 1 | Air quality monitoring |
| HC-SR501 PIR Sensor | 1 | Motion detection |
| OLED (0.96 inch) | 1 | Local display |
| RGB LED (Common Cathode) | 1 | Visual status indicator |
| 5V Relay Module | 1 | Fan control |
| 5V Fan | 1 | Ventilation |
| Active Buzzer | 1 | Audible alerts |
| LEDs (Red, Yellow, Green) | 3 | Alert indicators |
| Breadboards & Jumper Wires | - | Connections |

### **Power Requirements**
- **Input**: 5V DC via USB or external power supply
- **Current**: ~500mA (with all peripherals active)

## Wiring Diagram

### **ESP32 Pin Configuration**

| ESP32 Pin | Component | Signal | Notes |
|-----------|-----------|--------|-------|
| GPIO 14 | DHT22 | Data | Temperature & Humidity |
| GPIO 34 | MQ-135 | Analog | Gas Level (ADC) |
| GPIO 32 | PIR | Digital | Motion Detection |
| GPIO 23 | OLED | SDA | I2C Data |
| GPIO 22 | OLED | SCL | I2C Clock |
| GPIO 27 | RGB LED | Red | Common Cathode |
| GPIO 25 | RGB LED | Green | Common Cathode |
| GPIO 33 | RGB LED | Blue | Motion Indicator |
| GPIO 13 | Buzzer | Signal | Active Buzzer |
| GPIO 21 | LED | Red | Gas Alert |
| GPIO 19 | LED | Yellow | Temp Alert |
| GPIO 18 | LED | Green | Humidity Alert |
| GPIO 26 | Relay | Control | Fan ON/OFF |

![Scheme](AirGuard_scheme.png)

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
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
```

### **Library Installation**
1. Open Arduino IDE
2. Go to **Tools → Manage Libraries**
3. Search and install:
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `DHT sensor library`
   - `Universal Telegram Bot`
   - `ArduinoJson`

## 🤖 Telegram Bot Commands

| Command | Description | Example Response |
|---------|-------------|------------------|
| `/start` or `/help` | Show help message | Command list and system info |
| `/status` | Current system status | Uptime, WiFi, sensor states |
| `/sensors` | Real-time sensor data | Temperature, humidity, gas levels |
| `/alerts` | Active alerts | Gas, temperature, humidity alerts |
| `/id` | Show your Chat ID | Unique identifier for notifications |

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

- Predictive module with buffer and forecasting logic

- Test mode controller for scenario-based validation

- LED status monitoring for gas, temperature, and humidity alerts

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
