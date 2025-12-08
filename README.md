# Smart AirGuard - IoT Environmental Monitoring System

<div align="center">

![Smart AirGuard System](https://img.shields.io/badge/Platform-ESP32-blue)
![Telegram Bot](https://img.shields.io/badge/Telegram-Bot-blue)
![ThingSpeak](https://img.shields.io/badge/Cloud-ThingSpeak-orange)
![License](https://img.shields.io/badge/License-MIT-green)

*A comprehensive IoT-based environmental monitoring and alert system with real-time notifications*

</div>

## 📋 Table of Contents
- [Overview](#-overview)
- [Features](#-features)
- [System Architecture](#-system-architecture)
- [Hardware Requirements](#-hardware-requirements)
- [Wiring Diagram](#-wiring-diagram)
- [Software Dependencies](#-software-dependencies)
- [Installation & Setup](#-installation--setup)
- [Configuration](#-configuration)
- [Telegram Bot Commands](#-telegram-bot-commands)
- [ThingSpeak Integration](#thingspeak-integration)
- [Alert System](#-alert-system)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)
- [License](#-license)

## 🎯 Overview

Smart AirGuard is an intelligent environmental monitoring system built on ESP32 that continuously tracks air quality, temperature, humidity, and motion. It provides real-time alerts via Telegram and visual indicators when environmental parameters exceed safe thresholds.

## ✨ Features

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

## 🏗️ System Architecture

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

## 🔧 Hardware Requirements

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

## 🔌 Wiring Diagram

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

### **Power Connections**
- **3.3V**: DHT22, OLED, PIR sensor
- **5V**: MQ-135, Buzzer, LEDs (via resistors), Relay module
- **GND**: All components

## 📦 Software Dependencies

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

## 🚀 Installation & Setup

### **Step 1: Hardware Assembly**
1. Connect all sensors according to the wiring diagram
2. Ensure proper power connections
3. Double-check all ground connections

### **Step 2: Software Configuration**
1. Clone this repository
2. Open `SmartAirGuard.ino` in Arduino IDE
3. Configure your settings:

### **Step 3: Network Configuration**
```cpp
// WiFi Credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *pass = "YOUR_WIFI_PASSWORD";

// ThingSpeak Configuration
String apiKey = "YOUR_THINGSPEAK_API_KEY";
const char* server = "api.thingspeak.com";

// Telegram Bot Token
#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
```

### **Step 4: Threshold Configuration**
```cpp
// Alert Thresholds
#define DANGEROUS_GAS 400        // ppm
#define LOW_TEMP_THRESHOLD 10    // °C
#define HIGH_TEMP_THRESHOLD 35   // °C
#define HIGH_HUMIDITY_THRESHOLD 90  // %

// Timing Configuration
#define THINGSPEAK_DELAY 15000   // 15 seconds
#define ALERT_COOLDOWN 30000     // 30 seconds
#define MOTION_TIMEOUT 10000     // 10 seconds
```

## ⚙️ Configuration

### **Telegram Bot Setup**
1. Create a bot via [@BotFather](https://t.me/botfather)
2. Get your bot token
3. Update `BOT_TOKEN` in the code
4. Start the system and send `/start` to your bot

### **ThingSpeak Setup**
1. Create a [ThingSpeak](https://thingspeak.com) account
2. Create a new channel with 8 fields:
   - Field 1: Temperature
   - Field 2: Humidity
   - Field 3: Gas Level
   - Field 4: Motion Detected
   - Field 5: Gas Alert
   - Field 6: Temperature Alert
   - Field 7: Humidity Alert
   - Field 8: Fan Status
3. Copy your Write API Key

## 🤖 Telegram Bot Commands

| Command | Description | Example Response |
|---------|-------------|------------------|
| `/start` or `/help` | Show help message | Command list and system info |
| `/status` | Current system status | Uptime, WiFi, sensor states |
| `/sensors` | Real-time sensor data | Temperature, humidity, gas levels |
| `/alerts` | Active alerts | Gas, temperature, humidity alerts |
| `/id` | Show your Chat ID | Unique identifier for notifications |

### **Automatic Alerts**
The system automatically sends alerts for:
- 🚨 **Gas**: Level > 400 ppm
- 🌡️ **Temperature**: Outside 10-35°C range
- 💧 **Humidity**: Above 90%
- 🚶 **Motion**: When detected

## 📊 ThingSpeak Integration

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

### **Dashboard Configuration**
Create widgets in ThingSpeak to visualize:
- Temperature & Humidity graphs
- Gas level history
- Motion detection timeline
- Alert status indicators

## 🚨 Alert System

### **Priority Levels**
1. **CRITICAL** (Red LED + Buzzer + Telegram + Fan)
   - Gas level > 400 ppm
   - Immediate fan activation

2. **WARNING** (Yellow LED + Telegram)
   - Temperature outside range
   
3. **NOTICE** (Green LED + Telegram)
   - High humidity detected
   
4. **INFO** (Telegram only)
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
- Time until next ThingSpeak update
- Time until next Telegram check
- Fan status

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
