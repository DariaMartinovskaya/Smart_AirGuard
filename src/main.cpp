#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <MQ135.h>

// ---------------- WIFI + THINGSPEAK -----------------
const char *ssid = "Alex";
const char *pass = "Sacha3232";
String apiKey = "6QOIQZ7YFHAG6231";  
const char* server = "api.thingspeak.com";

// ---------------- ADAFRUIT IO (MQTT) -----------------
#define AIO_SERVER   "io.adafruit.com"
#define AIO_PORT     1883
#define AIO_USERNAME "DaryaMartsinouskaya"
#define AIO_KEY      "aio_DBBy73fmNk0xVVrV1foYz6qCFQmx"

// ---------------- OBJECTS -----------------
WiFiClient mqttClient;           // For Adafruit IO (MQTT)
HTTPClient http;                 // For ThingSpeak

// ---------------- MQTT -----------------
Adafruit_MQTT_Client mqtt(
  &mqttClient,
  AIO_SERVER,
  AIO_PORT,
  AIO_USERNAME,
  AIO_KEY
);

Adafruit_MQTT_Publish feedTemp   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish feedHum    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish feedGas    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/gas");
Adafruit_MQTT_Publish feedMotion = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/motion");
Adafruit_MQTT_Publish feedFan    = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/fan");
Adafruit_MQTT_Subscribe feedFanControl = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/fan");
Adafruit_MQTT_Publish feedEvent = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/events"); // New for Telegram
Adafruit_MQTT_Subscribe feedTelegramCommands = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/telegram_commands"); // New for Telegram

// ---------------- TIMERS -----------------
unsigned long lastThingSpeakUpdate = 0;
const unsigned long THINGSPEAK_DELAY = 15000; // 15 secs

unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_DELAY = 1000; // 1 sec

unsigned long lastMQTTUpdate = 0; 

float lastSentTemp = -100;        // Impossible starting value
float lastSentHum = -100;         // Impossible starting value
int lastSentGas = -1;             // Impossible starting value
bool lastSentMotion = false;
bool lastSentFan = false;
unsigned long lastForceSend = 0;  // Forced sending once a minute
const unsigned long FORCE_SEND_INTERVAL = 60000; // 60 secs

bool fanManualOverride = false;   // true = manual config from Node-RED
bool fanDesiredState = false;     // Intended fan status (for mannual regime)
bool gasEmergency = false;        // true = gas emergency
bool gasEmergencyActive = false;  // true = now is emergency
bool manualBeforeEmergency = false; // condition is saved before emergency
bool desiredStateBeforeEmergency = false;
unsigned long lastFanToggleTime = 0;
const unsigned long FAN_DEBOUNCE = 1000; 
unsigned long gasNormalizedTime = 0;
const unsigned long GAS_NORMAL_DELAY = 10000; // 10 secs after gas level normalization 

unsigned long lastGasAlertTime = 0;
unsigned long lastTempAlertTime = 0;
unsigned long lastHumidityAlertTime = 0;
unsigned long lastMotionAlertTime = 0;
const unsigned long ALERT_COOLDOWN = 30000;

unsigned long autoModeRestoreTime = 0;
const unsigned long AUTO_RESTORE_DELAY = 300000; // 5 min (300 000 ms)

String lastFanCommand = "";
unsigned long lastFanCommandTime = 0;
const unsigned long FAN_COMMAND_COOLDOWN = 3000; 


// ---------------- PINS -----------------
#define DHTPIN 14
#define BUZZER_PIN 13     
#define RED_LED 21
#define YELLOW_LED 19
#define GREEN_LED 18
#define MQ135_PIN 34
#define RELAY_PIN 26       
#define PIR_PIN 32         

#define RGB_RED 27       
#define RGB_GREEN 25      
#define RGB_BLUE 33

#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define DANGEROUS_GAS 1000
#define LOW_TEMP_THRESHOLD 10
#define HIGH_TEMP_THRESHOLD 35
#define HIGH_HUMIDITY_THRESHOLD 90

#define PIR_DEBOUNCE_TIME 2000
#define MOTION_TIMEOUT 10000
#define BLINK_INTERVAL 500

// ---------------- OBJECTS -----------------
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
MQ135 mq135 = MQ135(MQ135_PIN);  // Creating sensor object

// ---------------- MQ-135 CALIBRATION VALUES -----------------
float mq135_RZero = 39;           // For storaging calibrated value R0
bool calibrationDone = false;    // Flag that calibration is finished

// ---------------- VARIABLES -----------------
String chatId = "";  
bool chatIdFound = false;

float temperature = 0;
float humidity = 0;
int gasLevel = 0;

bool gasAlert = false;
bool tempAlert = false;
bool humidityAlert = false;
bool motionDetected = false;

unsigned long lastMotionTime = 0;
unsigned long pirReadyTime = 0;
unsigned long lastBlinkTime = 0;
bool blueLedState = false;

// Flags
bool lastGasAlert = false;
bool lastTempAlert = false;
bool lastHumidityAlert = false;
bool lastMotionState = false;  


// ---------------- FUNCTION PROTOTYPES -----------------
void readSensors();
void sendToThingSpeak();
void sendToAdafruitIO();
void checkMotion();
void checkConditions();
void displayData();
void updateRGBLed();
void setRGBColor(bool red, bool green, bool blue);
void toneAlertNonBlocking();
void MQTT_connect();
void handleMQTTCommands();
void updateFanState(); 
void publishEventsToMQTT();

// Telegram functions
void setupTelegram();
void handleTelegramMessages();
String getUptime();

// Function for obtaining calibrated gas value (ppm CO2)
float getCalibratedGasPPM() {
  if (!calibrationDone) {
    Serial.println("⚠️ Warning: MQ135 not calibrated yet!");
    return -1;
  }
  
  // Get the sensor resistance
  float resistance = mq135.getResistance();
  
  // Calculate the Rs/R0 ratio
  float ratio = resistance / mq135_RZero;
  
  // Simpler formula:
  float ppm = 400 * (3.6 / ratio);
  
  // Limit the value
  if (ppm < 400) ppm = 400;
  if (ppm > 5000) ppm = 5000;
  
  static unsigned long lastDebugLog = 0;
  if (millis() - lastDebugLog > 2000) {
    Serial.print("🔍 [MQ135] Rs=");
    Serial.print(resistance, 1);
    Serial.print(" Ω, R0=");
    Serial.print(mq135_RZero, 1);
    Serial.print(", Ratio=");
    Serial.print(ratio, 3);
    Serial.print(", PPM=");
    Serial.println(ppm, 0);
    lastDebugLog = millis();
  }
  
  return ppm;
}

// Function for obtaining raw resistance (for debugging)
float getGasResistance() {
  return mq135.getResistance();
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Wire.begin(23, 22);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH); // HIGH = OFF (active LOW level)
  setRGBColor(false, false, false);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR!");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("System Starting...");
  display.println("Initializing sensors...");
  display.display();

  delay(1000);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  int retry = 0;
  while ((isnan(temperature) || isnan(humidity)) && retry < 5) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Reading DHT22...");
    display.println("Attempt: " + String(retry + 1));
    display.display();
    
    delay(1000);
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    retry++;
  }

  mq135_RZero = 39;
  calibrationDone = true;
  Serial.print("✅ Using calibrated RZERO = ");
  Serial.println(mq135_RZero);
  pirReadyTime = millis() + PIR_DEBOUNCE_TIME;

  // RGB test
  setRGBColor(true, false, false); delay(300);
  setRGBColor(false, true, false); delay(300);
  setRGBColor(false, false, true); delay(300);
  setRGBColor(false, false, false);
               
  mqtt.subscribe(&feedTelegramCommands);

  mqtt.subscribe(&feedFanControl);
  
  Serial.println("System ready! Fan control: Automatic (gas sensor)");
  Serial.println("RELAY_PIN: HIGH = OFF, LOW = ON");
}

void loop() {
  readSensors();
  checkMotion();
  updateRGBLed();    
  checkConditions();
  updateFanState(); 
  displayData();
  
  if (millis() - lastThingSpeakUpdate >= THINGSPEAK_DELAY) {
    sendToThingSpeak();
    lastThingSpeakUpdate = millis();
  }
  
  sendToAdafruitIO();
  handleMQTTCommands();
  
  // Handle MQTT commands from Node-RED
  // publishEventsToMQTT(); // New for Telegram
  
  toneAlertNonBlocking(); // non-blocking buzzer
  delay(100);
}

// ---------------- SENSOR FUNCTIONS -----------------
unsigned long lastDHTread = 0;
const unsigned long DHT_INTERVAL = 2000;

void readSensors() {
  if (millis() - lastDHTread < DHT_INTERVAL) return;
  lastDHTread = millis();

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (calibrationDone) {
    gasLevel = (int)getCalibratedGasPPM();  
  } else {
    gasLevel = analogRead(MQ135_PIN);  
  }
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error reading DHT22!");
    return;
  }
  
  Serial.print("Temp: "); Serial.print(temperature);
  Serial.print("C, Humidity: "); Serial.print(humidity);
  Serial.print("%, Gas (ppm): "); Serial.print(gasLevel);
  Serial.print(", Motion: "); Serial.println(motionDetected ? "YES" : "NO");
  Serial.print("Fan Override: "); Serial.println(fanManualOverride ? "MANUAL" : "AUTO");
  Serial.print("Fan Desired: "); Serial.println(fanDesiredState ? "ON" : "OFF");
  Serial.print("Gas Emergency Active: "); Serial.println(gasEmergencyActive ? "YES" : "NO");
  Serial.print("Manual Before: "); Serial.println(manualBeforeEmergency ? "YES" : "NO");
  Serial.print("Relay PIN: "); Serial.println(digitalRead(RELAY_PIN) == LOW ? "LOW (ON)" : "HIGH (OFF)");
}

void updateFanState() {
  bool shouldFanBeOn = false;
  
// ============= PRIORITY 1: GAS EMERGENCY =============
// Gas has the highest priority - we turn on the fan ALWAYS at a dangerous level
  if (gasLevel > DANGEROUS_GAS) {
    shouldFanBeOn = true;
    gasEmergency = true;
    
// Save the state before the crash, if this is the beginning of the crash
    if (!gasEmergencyActive) {
      gasEmergencyActive = true;
      manualBeforeEmergency = fanManualOverride;
      desiredStateBeforeEmergency = fanDesiredState;
      Serial.println("⚠️ GAS EMERGENCY STARTED! Saving current state.");

      autoModeRestoreTime = 0;
    }
    
    // If there was manual mode and the fan was off, we send a warning
    if (fanManualOverride && !fanDesiredState) {
      static unsigned long lastGasOverrideWarning = 0;
      if (millis() - lastGasOverrideWarning > 60000) { // Once a minute
        Serial.println("⚠️ GAS EMERGENCY: Overriding manual OFF to ON!");
        lastGasOverrideWarning = millis();
      }
    }
  } 
  else {
    // gas is normal
    if (gasEmergencyActive) {
      // Save time of normalization
      if (gasNormalizedTime == 0) {
        gasNormalizedTime = millis();
        Serial.println("✅ Gas normalized. Waiting " + String(GAS_NORMAL_DELAY/1000) + " seconds before restoring control...");
      }
      
      if (millis() - gasNormalizedTime >= GAS_NORMAL_DELAY) {
        gasEmergency = false;
        gasEmergencyActive = false;
        
        fanManualOverride = manualBeforeEmergency;
        fanDesiredState = desiredStateBeforeEmergency;

        String mqttMsg = "GAS_NORMAL:" + String(gasLevel);
        if (feedEvent.publish(mqttMsg.c_str())) {
            Serial.println("📤 GAS_NORMAL sent to MQTT (emergency ended)");
        }
        
        Serial.println("✅ Gas emergency ended. Restoring control to user.");
        Serial.print("Restored state - Manual: ");
        Serial.print(fanManualOverride ? "YES" : "NO");
        Serial.print(", Desired: ");
        Serial.println(fanDesiredState ? "ON" : "OFF");

        if (fanManualOverride) {
          autoModeRestoreTime = millis() + AUTO_RESTORE_DELAY;
          Serial.print("⏱️ Auto mode will restore in ");
          Serial.print(AUTO_RESTORE_DELAY / 60000);
          Serial.println(" minutes if no further manual commands");
        }
        
        if (chatIdFound) {
          String modeMsg = fanManualOverride ? 
            (fanDesiredState ? "Manual ON" : "Manual OFF") : "Automatic mode";
        }
        
        gasNormalizedTime = 0;
      }
    } else {
      gasEmergency = false;
    }
    
    // ============ PRIORITY 2: MANUAL ============
    if (fanManualOverride && !gasEmergency) {
      shouldFanBeOn = fanDesiredState;
      // Check the automatic restore timer for AUTO mode
      if (autoModeRestoreTime > 0 && millis() > autoModeRestoreTime) {
        Serial.println("🔄 Auto mode restored automatically after timeout");
        fanManualOverride = false;
        fanDesiredState = false;
        autoModeRestoreTime = 0;
        
        // Send AUTO command to MQTT to sync with Node-RED
        if (mqtt.connected()) {
          feedFan.publish("AUTO");
          Serial.println("📤 Sent AUTO command to MQTT");
        }
        
        if (chatIdFound) {
          // // Recalculate fan state after switching mode
          shouldFanBeOn = false; // In AUTO mode, the fan is off when gas level is normal
        }
      }
    } 
    // ============ PRIORITY 3: AUTO ============
    else if (!gasEmergency) {
      shouldFanBeOn = false; // In automatic mode without gas - off
    
      // If in AUTO mode and gas level is normal - fan is off
      if (fanManualOverride == false) {
        // // Make sure the relay is turned off
        digitalWrite(RELAY_PIN, HIGH);
      }
    }
  }
  
  // Apply the state to the relay (LOW = on, HIGH = off)
  if (shouldFanBeOn) {
    digitalWrite(RELAY_PIN, LOW);
  } else {
    digitalWrite(RELAY_PIN, HIGH);
  }
}


// ---------------- THINGSPEAK -----------------
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient clientTS;
    HTTPClient httpTS;
    String url = "http://" + String(server) + "/update";
    url += "?api_key=" + apiKey;
    url += "&field1=" + String(temperature);
    url += "&field2=" + String(humidity);
    url += "&field3=" + String(gasLevel);
    url += "&field4=" + String(motionDetected ? 1 : 0);
    url += "&field5=" + String(gasAlert ? 1 : 0);
    url += "&field6=" + String(tempAlert ? 1 : 0);
    url += "&field7=" + String(humidityAlert ? 1 : 0);
    url += "&field8=" + String(digitalRead(RELAY_PIN) == LOW ? 1 : 0);

    Serial.print("Sending to ThingSpeak: "); Serial.println(url);
    httpTS.begin(clientTS, url);
    int httpCode = httpTS.GET();
    if (httpCode > 0) {
      Serial.print("ThingSpeak HTTP code: "); Serial.println(httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String payload = httpTS.getString();
        Serial.print("Response: "); Serial.println(payload);
      }
    } else {
      Serial.print("❌ Error: "); Serial.println(httpTS.errorToString(httpCode).c_str());
    }
    httpTS.end();
  } else {
    Serial.println("❌ WiFi disconnected!");
  }
}

// ---------------- ADAFRUIT IO (MQTT) -----------------
void MQTT_connect() {
  int8_t ret;
  
  if (mqtt.connected()) {
    return;
  }
  
  Serial.print("Connecting to Adafruit IO... ");
  
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      Serial.println("MQTT connection failed!");
      return;
    }
  }
  
  Serial.println("Adafruit IO connected!");
}

void sendToAdafruitIO() {
  MQTT_connect();
  
  if (!mqtt.ping()) {
    Serial.println("MQTT ping failed, reconnecting...");
    mqtt.disconnect();
    MQTT_connect();
  }
  
  bool changed = false;
  
  // The temperature is only sent if it has changed by more than 0.5°C.
  if (abs(temperature - lastSentTemp) > 0.5) {
    if (feedTemp.publish(temperature)) {
      Serial.print("[MQTT] Temp changed: "); Serial.println(temperature);
      lastSentTemp = temperature;
      changed = true;
      lastMQTTUpdate = millis();
    }
  }
  
  // Humidity data is only sent if it has changed by more than 1%.
  if (abs(humidity - lastSentHum) > 1.0) {
    if (feedHum.publish(humidity)) {
      Serial.print("[MQTT] Hum changed: "); Serial.println(humidity);
      lastSentHum = humidity;
      changed = true;
      lastMQTTUpdate = millis();
    }
  }
  
  // Gas is only sent if the change is more than 50 units.
  if (abs(gasLevel - lastSentGas) > 50) {
    if (feedGas.publish(gasLevel)) {
      Serial.print("[MQTT] Gas changed: "); Serial.println(gasLevel);
      lastSentGas = gasLevel;
      changed = true;
      lastMQTTUpdate = millis();
    }
  }
  
  // The movement is sent only if the status has changed.
  if (motionDetected != lastSentMotion) {
    String motionStr = motionDetected ? "DETECTED" : "NO MOTION";
    if (feedMotion.publish(motionStr.c_str())) {
      Serial.print("[MQTT] Motion changed: "); Serial.println(motionStr);
      lastSentMotion = motionDetected;
      changed = true;
      lastMQTTUpdate = millis();
    }
  }
  
  // The fan status is sent only if the status has changed.
  bool currentFan = (digitalRead(RELAY_PIN) == LOW);
  if (currentFan != lastSentFan) {
    String fanStr = currentFan ? "ON" : "OFF";
    if (feedFan.publish(fanStr.c_str())) {
      Serial.print("[MQTT] Fan changed: "); Serial.println(fanStr);
      lastSentFan = currentFan;
      changed = true;
      lastMQTTUpdate = millis();
    }
  }
  
// If nothing has changed, send the minimum data every 60 seconds
// to maintain the connection and ensure Node-RED has fresh data
  if (!changed && (millis() - lastForceSend > FORCE_SEND_INTERVAL)) {
    // Temperature is sent as an indicator of operation.
    if (feedTemp.publish(temperature)) {
      Serial.print("[MQTT] Force update sent (temp): "); Serial.println(temperature);
      lastForceSend = millis();
      lastMQTTUpdate = millis();
    }
  }
}

// ---------------- MQTT COMMAND HANDLER -----------------
void handleMQTTCommands() {
  Adafruit_MQTT_Subscribe *subscription;
  
  while ((subscription = mqtt.readSubscription(0))) {
    
    // ========== FAN COMMAND HANDLING ==========
    if (subscription == &feedFanControl) {
      if (millis() - lastFanToggleTime < FAN_DEBOUNCE) {
        return;
      }

      
      
      String fanCommand = (char *)feedFanControl.lastread;

      if (fanCommand == lastFanCommand && (millis() - lastFanCommandTime) < FAN_COMMAND_COOLDOWN) {
        Serial.print("⚠️ Duplicate command ignored: ");
        Serial.println(fanCommand);
        return;
      }
      
      lastFanCommand = fanCommand;
      lastFanCommandTime = millis();

      Serial.print("Fan control received from MQTT: ");
      Serial.println(fanCommand);
      
      if (fanCommand == "ON") {
        if (fanManualOverride && fanDesiredState == true) {
          Serial.println("⚠️ Fan already ON in manual mode, ignoring duplicate");
          return;
        }

        fanManualOverride = true;
        fanDesiredState = true;
        lastFanToggleTime = millis();
        Serial.println("✅ Fan turned ON from Node-RED (Manual mode)");
        // feedFan.publish("ON");
      } 
      else if (fanCommand == "OFF") {
        if (fanManualOverride && fanDesiredState == false) {
          Serial.println("⚠️ Fan already OFF in manual mode, ignoring duplicate");
          return;
        }

        if (gasLevel > DANGEROUS_GAS) {
          Serial.println("⚠️ Cannot turn fan OFF - GAS EMERGENCY!");
          feedFan.publish("ON");
          return;
        }
        
        fanManualOverride = true;
        fanDesiredState = false;
        lastFanToggleTime = millis();
        Serial.println("✅ Fan turned OFF from Node-RED (Manual mode)");
        // feedFan.publish("OFF");
      } 
      else if (fanCommand == "AUTO") {
        if (!fanManualOverride) {
          Serial.println("⚠️ Already in AUTO mode, ignoring duplicate");
          return;
        }
        
        fanManualOverride = false;
        lastFanToggleTime = millis();
        Serial.println("🔄 Fan control switched to AUTO mode (gas sensor)");
      }
    }
    
    // ========== HANDLING COMMANDS FROM TELEGRAM ==========
if (subscription == &feedTelegramCommands) {
    String command = (char *)feedTelegramCommands.lastread;
    Serial.print("📱 Telegram command received: ");
    Serial.println(command);
    
    String response = "";
    
    if (command == "STATUS") {
        response = "📊 *System Status*\n\n";
        response += "• Gas: " + String(gasLevel) + " ppm";
        if (gasLevel > DANGEROUS_GAS) response += " 🔴 DANGER";
        else if (gasLevel > 600) response += " 🟡 ELEVATED";
        else response += " ✅ NORMAL";
        response += "\n";
        response += "• Temperature: " + String(temperature) + "°C";
        if (temperature < LOW_TEMP_THRESHOLD || temperature > HIGH_TEMP_THRESHOLD) response += " ⚠️ ALERT";
        response += "\n";
        response += "• Humidity: " + String(humidity) + "%";
        if (humidity > HIGH_HUMIDITY_THRESHOLD) response += " 💧 HIGH";
        response += "\n";
        response += "• Motion: " + String(motionDetected ? "🚶 DETECTED" : "⚫ NONE") + "\n";
        response += "• Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "🌀 ON" : "⭕ OFF") + "\n";
        response += "• Fan Mode: " + String(fanManualOverride ? "🔄 MANUAL" : "⚙️ AUTO");
    }
    else if (command == "SENSORS") {
        response = "📡 *Sensor Data*\n\n";
        response += "🌡️ Temperature: *" + String(temperature) + "°C*\n";
        response += "💧 Humidity: *" + String(humidity) + "%*\n";
        response += "⚠️ Gas: *" + String(gasLevel) + " ppm*\n";
        response += "🚶 Motion: *" + String(motionDetected ? "YES" : "NO") + "*";
    }
    else if (command == "ALERTS") {
        response = "🚨 *Active Alerts*\n\n";
        bool hasAlert = false;
        if (gasLevel > DANGEROUS_GAS) {
            response += "🔴 GAS DANGER! " + String(gasLevel) + " ppm\n";
            hasAlert = true;
        }
        if (tempAlert) {
            response += "🌡️ TEMPERATURE: " + String(temperature) + "°C\n";
            hasAlert = true;
        }
        if (humidityAlert) {
            response += "💧 HIGH HUMIDITY: " + String(humidity) + "%\n";
            hasAlert = true;
        }
        if (motionDetected) {
            response += "🚶 MOTION DETECTED\n";
            hasAlert = true;
        }
        if (!hasAlert) response = "✅ *No active alerts*\nAll parameters are normal.";
    }
    else if (command == "HELP") {
        response = "📱 *Available Commands*\n\n";
        response += "/status - System status\n";
        response += "/sensors - Sensor data\n";
        response += "/alerts - Active alerts\n";
        response += "/help - This message\n\n";
        response += "🔄 *Fan Control*\n";
        response += "/fan_on - Turn fan ON\n";
        response += "/fan_off - Turn fan OFF\n";
        response += "/fan_auto - Return to AUTO mode\n\n";
        response += "📍 Smart AirGuard System";
    }
    else if (command == "FAN_ON") {
        if (gasLevel > DANGEROUS_GAS) {
            response = "⚠️ *Cannot turn fan ON!*\n\n";
            response += "Gas emergency is ACTIVE!\n";
            response += "Fan is already ON for safety.\n";
            response += "Gas level: " + String(gasLevel) + " ppm";
        } else {
            fanManualOverride = true;
            fanDesiredState = true;
            response = "🌀 *Fan turned ON*\n\n";
            response += "Fan is now running.\n";
            response += "Mode: MANUAL\n";
            response += "Use /fan_off to turn OFF or /fan_auto for AUTO mode.";
            feedFan.publish("ON");
        }
    }
    else if (command == "FAN_OFF") {
        if (gasLevel > DANGEROUS_GAS) {
            response = "⚠️ *Cannot turn fan OFF!*\n\n";
            response += "Gas emergency is ACTIVE!\n";
            response += "Fan forced ON for safety.\n";
            response += "Gas level: " + String(gasLevel) + " ppm";
        } else {
            fanManualOverride = true;
            fanDesiredState = false;
            response = "⭕ *Fan turned OFF*\n\n";
            response += "Fan is now stopped.\n";
            response += "Mode: MANUAL\n";
            response += "Use /fan_on to turn ON or /fan_auto for AUTO mode.";
            feedFan.publish("OFF");
        }
    }
    else if (command == "FAN_AUTO") {
        fanManualOverride = false;
        response = "⚙️ *Fan switched to AUTO mode*\n\n";
        response += "Fan will now be controlled by gas sensor.\n";
        response += "Current gas level: " + String(gasLevel) + " ppm";
    }
    
    // Sending response
    if (response != "") {
        Adafruit_MQTT_Publish feedTelegramResponse = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/telegram_response");
        feedTelegramResponse.publish(response.c_str());
        Serial.println("📤 Response sent to telegram_response");
    }
    }
  }
}

// ---------------- MOTION -----------------
void checkMotion() {
  if (millis() < pirReadyTime) return;
  int pirState = digitalRead(PIR_PIN);
  
  if (pirState == HIGH) {
    if (!motionDetected) {
      motionDetected = true;
      lastMotionTime = millis();
      lastBlinkTime = millis();
      Serial.println("Motion detected!");
    } else {
      lastMotionTime = millis();
    }
  } else {
    if (motionDetected && (millis() - lastMotionTime > MOTION_TIMEOUT)) {
      motionDetected = false;
      blueLedState = false;
      setRGBColor(false, false, false);
      Serial.println("Motion timeout - Blue LED turned OFF");
    }
  }
}


void updateRGBLed() {
  if (motionDetected) {
    if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
      blueLedState = !blueLedState;
      setRGBColor(false, false, blueLedState);
      lastBlinkTime = millis();
    }
  } else if (blueLedState) {
    blueLedState = false;
    setRGBColor(false, false, false);
  }
}

void setRGBColor(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE, blue ? HIGH : LOW);
  if (blue) Serial.println("setRGBColor: Blue ON");
}

// ---------------- ALERTS -----------------
unsigned long lastBuzzerTime = 0;
const unsigned long BUZZER_DURATION = 1000;
bool buzzerActive = false;

void toneAlertNonBlocking() {
  // The siren operates in the presence of dangerous gas regardless of the mode
  if (gasLevel > DANGEROUS_GAS) {
    if (!buzzerActive) {
      digitalWrite(BUZZER_PIN, HIGH);
      lastBuzzerTime = millis();
      buzzerActive = true;
    } else if (millis() - lastBuzzerTime >= BUZZER_DURATION) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerActive = false;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

// ---------------- CONDITIONS -----------------
void checkConditions() {
  bool prevGasAlert = lastGasAlert;
  bool prevTempAlert = lastTempAlert;
  bool prevHumidityAlert = lastHumidityAlert;
  bool prevMotionState = lastMotionState;

  gasAlert = false;
  
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  // ========== GAS ==========
  if (gasLevel > DANGEROUS_GAS) {
    gasAlert = true;
    digitalWrite(RED_LED, HIGH);

    if (millis() - lastGasAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "GAS_ALERT:" + String(gasLevel);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastGasAlertTime = millis();
    }
  } 
  else if (prevGasAlert && gasLevel <= DANGEROUS_GAS) {
    // Only send ONCE when gas normalizes
    if (millis() - lastGasAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "GAS_NORMAL:" + String(gasLevel);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastGasAlertTime = millis();  // Use same timer to prevent repeated sends
    }
  }

  // ========== TEMPERATURE ==========
  if (temperature < LOW_TEMP_THRESHOLD || temperature > HIGH_TEMP_THRESHOLD) {
    tempAlert = true;
    digitalWrite(YELLOW_LED, HIGH);
    
    if (millis() - lastTempAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "TEMP_ALERT:" + String(temperature);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastTempAlertTime = millis();
    }
  } 
  else if (prevTempAlert) {
    // Only send ONCE when temperature normalizes
    if (millis() - lastTempAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "TEMP_NORMAL:" + String(temperature);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastTempAlertTime = millis();  // Use same timer to prevent repeated sends
    }
  }

  // ========== HUMIDITY ==========
  if (humidity > HIGH_HUMIDITY_THRESHOLD) {
    humidityAlert = true;
    digitalWrite(GREEN_LED, HIGH);
    
    if (millis() - lastHumidityAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "HUMIDITY_ALERT:" + String(humidity);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastHumidityAlertTime = millis();
    }
  } 
  else if (prevHumidityAlert) {
    // Only send ONCE when humidity normalizes
    if (millis() - lastHumidityAlertTime > ALERT_COOLDOWN) {
      String mqttMsg = "HUMIDITY_NORMAL:" + String(humidity);
      if (feedEvent.publish(mqttMsg.c_str())) {
        Serial.print("📤 [MQTT] Published event: ");
        Serial.println(mqttMsg);
      }
      lastHumidityAlertTime = millis();  // Use same timer to prevent repeated sends
    }
  }

  // ========== MOTION ==========
  static unsigned long lastMotionPublish = 0;

  if (motionDetected && !prevMotionState &&
    millis() - lastMotionPublish > 10000) {

    lastMotionPublish = millis();
    String mqttMsg = "MOTION_DETECTED";
    if (feedEvent.publish(mqttMsg.c_str())) {
      Serial.print("📤 [MQTT] Published event: ");
      Serial.println(mqttMsg);
    }
    lastMotionAlertTime = millis();
  } 
  else if (!motionDetected && prevMotionState && (millis() - lastMotionAlertTime > 5000)) {
    // Motion stopped - only send once
    String mqttMsg = "MOTION_STOPPED";
    if (feedEvent.publish(mqttMsg.c_str())) {
      Serial.print("📤 [MQTT] Published event: ");
      Serial.println(mqttMsg);
    }
    // Don't update lastMotionAlertTime here to allow cooldown
  }

  lastGasAlert = gasAlert;
  lastTempAlert = tempAlert;
  lastHumidityAlert = humidityAlert;
  lastMotionState = motionDetected;
}

// ---------------- DISPLAY -----------------
void displayData() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  
  display.println("ENVIRONMENT MONITOR");
  display.println("-------------------");
  display.print("Temp: "); display.print(temperature, 1); display.println(" C");
  display.print("Humidity: "); display.print(humidity, 1); display.println(" %");
  display.print("Gas Level: "); display.println(gasLevel);
  display.print("Motion: "); display.println(motionDetected ? "DETECTED" : "NONE");
  display.println("-------------------");
  
  display.print("Fan Mode: ");
  if (gasEmergencyActive) {
    display.println("GAS EMERGENCY!");
    display.print("Gas norm in: ");
    if (gasNormalizedTime > 0) {
      int secondsLeft = GAS_NORMAL_DELAY - (millis() - gasNormalizedTime);
      display.print(max(0, secondsLeft / 1000));
      display.println("s");
    } else {
      display.println("--");
    }
  } else {
    display.println(fanManualOverride ? "MANUAL" : "AUTO");
  }
  
  if (gasLevel > DANGEROUS_GAS) display.println("! GAS DANGER !");
  if (tempAlert) display.println("! TEMP ALERT !");
  if (humidityAlert) display.println("! HUMID HIGH !");
  
  display.print("Fan: "); display.println(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF");
  
  if (motionDetected) {
    display.print("Last motion: ");
    display.print((millis() - lastMotionTime) / 1000);
    display.println("s ago");
  }
  
  display.print("Blue LED: "); display.println(blueLedState ? "ON" : "OFF");
  
  display.print("TS: "); display.print(max(0, (int)(THINGSPEAK_DELAY - (millis() - lastThingSpeakUpdate)) / 1000)); display.println("s");
  display.print("TG: "); display.print(max(0, (int)(TELEGRAM_DELAY - (millis() - lastTelegramCheck)) / 1000)); display.println("s");
  
  display.print("MQTT last: ");
  if (lastMQTTUpdate == 0) {
    display.println("Never");
  } else {
    display.print((millis() - lastMQTTUpdate) / 1000);
    display.println("s ago");
  }
  
  display.display();
}

// void publishEventsToMQTT() {
    
//     static bool lastGasAlertPublished = false;
//     static bool lastTempAlertPublished = false;
//     static bool lastHumidityAlertPublished = false;
//     static bool lastMotionPublished = false;
    
//     // Publish gas alert
//     if (gasAlert && !lastGasAlertPublished) {
//         String mqttMsg = "GAS_ALERT:" + String(gasLevel);
//         if (feedEvent.publish(mqttMsg.c_str())) {  
//             Serial.println("📤 GAS_ALERT sent to MQTT");
//             lastGasAlertPublished = true;
//         }
//     } else if (!gasAlert) {
//         lastGasAlertPublished = false;
//     }
    
//     // Publish temperature alert
//     if (tempAlert && !lastTempAlertPublished) {
//         String mqttMsg = "TEMP_ALERT:" + String(temperature);
//         if (feedEvent.publish(mqttMsg.c_str())) {  
//             Serial.println("📤 TEMP_ALERT sent to MQTT");
//             lastTempAlertPublished = true;
//         }
//     } else if (!tempAlert) {
//         lastTempAlertPublished = false;
//     }
    
//     // Publish humidity
//     if (humidityAlert && !lastHumidityAlertPublished) {
//         String mqttMsg = "HUMIDITY_ALERT:" + String(humidity);
//         if (feedEvent.publish(mqttMsg.c_str())) {  
//             Serial.println("📤 HUMIDITY_ALERT sent to MQTT");
//             lastHumidityAlertPublished = true;
//         }
//     } else if (!humidityAlert) {
//         lastHumidityAlertPublished = false;
//     }
    
//     // Publish motion
//     if (motionDetected && !lastMotionPublished) {
//         String mqttMsg = "MOTION_DETECTED";
//         if (feedEvent.publish(mqttMsg.c_str())) {  
//             Serial.println("📤 MOTION_DETECTED sent to MQTT");
//             lastMotionPublished = true;
//         }
//     } else if (!motionDetected) {
//         lastMotionPublished = false;
//     }
// }

String getUptime() {
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  if (days > 0) {
    return String(days) + "d " + String(hours) + "h " + String(minutes) + "min";
  } else if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "min " + String(seconds) + "sec";
  } else {
    return String(minutes) + "min " + String(seconds) + "sec";
  }
}
