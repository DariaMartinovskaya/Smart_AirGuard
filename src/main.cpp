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
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <MQ135.h>

// ---------------- WIFI + THINGSPEAK -----------------
const char *ssid = "Alex";
const char *pass = "Sacha3232";
String apiKey = "6QOIQZ7YFHAG6231";  
const char* server = "api.thingspeak.com";

// ---------------- TELEGRAM BOT -----------------
#define BOT_TOKEN "8159417350:AAFVaCa_djh0A81HtL3QTT7Ww9rcLwBU4Yg"  

// ---------------- ADAFRUIT IO (MQTT) -----------------
#define AIO_SERVER   "io.adafruit.com"
#define AIO_PORT     1883
#define AIO_USERNAME "DaryaMartsinouskaya"
#define AIO_KEY      "aio_DBBy73fmNk0xVVrV1foYz6qCFQmx"

// ---------------- OBJECTS -----------------
WiFiClientSecure telegramClient; // For Telegram
WiFiClient mqttClient;           // For Adafruit IO (MQTT)
HTTPClient http;                 // For ThingSpeak
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

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

// Telegram functions
void setupTelegram();
void handleTelegramMessages();
void sendTelegramAlert(String message);
void sendStatus(String chat_id);
void sendSensorData(String chat_id);
String getUptime();

// Function for obtaining calibrated gas value (ppm CO2)
float getCalibratedGasPPM() {
  if (!calibrationDone) {
    Serial.println("⚠️ Warning: MQ135 not calibrated yet!");
    return -1;
  }
  
  // Получаем сопротивление датчика
  float resistance = mq135.getResistance();
  
  // Вычисляем отношение Rs/R0
  float ratio = resistance / mq135_RZero;
  
  // Альтернативная формула для CO2
  // При Rs/R0 = 3.6 должно быть 400 ppm
  // При Rs/R0 = 1.0 должно быть 1000 ppm
  // При Rs/R0 = 0.4 должно быть 2000 ppm
  
  // Более простая формула:
  float ppm = 400 * (3.6 / ratio);
  
  // Ограничиваем
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
  
  // Setup Telegram & clients
  telegramClient.setInsecure();  // Telegram
  setupTelegram();               

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
  
  if (millis() - lastTelegramCheck >= TELEGRAM_DELAY) {
    handleTelegramMessages();
    lastTelegramCheck = millis();
  }
  
  sendToAdafruitIO();
  
  // Handle MQTT commands from Node-RED
  handleMQTTCommands();
  
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
  // gasLevel = analogRead(MQ135_PIN);
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
  // Serial.print("%, Gas: "); Serial.print(gasLevel);
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
      Serial.print("Saved state - Manual: ");
      Serial.print(manualBeforeEmergency ? "YES" : "NO");
      Serial.print(", Desired: ");
      Serial.println(desiredStateBeforeEmergency ? "ON" : "OFF");
    }
    
    // If there was manual mode and the fan was off, we send a warning
    if (fanManualOverride && !fanDesiredState) {
      static unsigned long lastGasOverrideWarning = 0;
      if (millis() - lastGasOverrideWarning > 60000) { // Once a minute
        Serial.println("⚠️ GAS EMERGENCY: Overriding manual OFF to ON!");
        if (chatIdFound) {
          bot.sendMessage(chatId, "⚠️ *GAS EMERGENCY OVERRIDE!*\n\n🚨 Dangerous gas level detected: " + 
                          String(gasLevel) + "\n🌀 Fan forced ON for safety!\n⚙️ Manual control will be restored when gas normalizes", "Markdown");
        }
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
        
        Serial.println("✅ Gas emergency ended. Restoring control to user.");
        Serial.print("Restored state - Manual: ");
        Serial.print(fanManualOverride ? "YES" : "NO");
        Serial.print(", Desired: ");
        Serial.println(fanDesiredState ? "ON" : "OFF");
        
        if (chatIdFound) {
          String modeMsg = fanManualOverride ? 
            (fanDesiredState ? "Manual ON" : "Manual OFF") : "Automatic mode";
          bot.sendMessage(chatId, "✅ *Gas emergency ended!*\n\n🌀 Fan control restored\n⚙️ Mode: " + modeMsg + 
                          "\n📊 Gas level: " + String(gasLevel), "Markdown");
        }
        
        gasNormalizedTime = 0;
      }
    } else {
      gasEmergency = false;
    }
    
    // ============ PRIORITY 2: MANUAL ============
    if (fanManualOverride && !gasEmergency) {
      shouldFanBeOn = fanDesiredState;
    } 
    // ============ PRIORITY 3: AUTO ============
    else if (!gasEmergency) {
      shouldFanBeOn = false; // In automatic mode without gas - off
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
  
  // Check for incoming MQTT messages with 0 timeout (non-blocking)
  while ((subscription = mqtt.readSubscription(0))) {
    if (subscription == &feedFanControl) {
      if (millis() - lastFanToggleTime < FAN_DEBOUNCE) {
        return;
      }
      
      String fanCommand = (char *)feedFanControl.lastread;
      Serial.print("Fan control received from MQTT: ");
      Serial.println(fanCommand);
      
      if (fanCommand == "ON") {
        fanManualOverride = true; // Turning into manual regime
        fanDesiredState = true;   // Fan is turn on
        lastFanToggleTime = millis();
        Serial.println("✅ Fan turned ON from Node-RED (Manual mode)");
        
        feedFan.publish("ON");
        
        if (chatIdFound) {
          bot.sendMessage(chatId, "✅ Fan turned ON from Node-RED\n🔄 Mode: Manual control", "");
        }
      } 
      else if (fanCommand == "OFF") {
        if (gasLevel > DANGEROUS_GAS) {
          Serial.println("⚠️ Cannot turn fan OFF - GAS EMERGENCY!");
          
          feedFan.publish("ON");
          
          if (chatIdFound) {
            bot.sendMessage(chatId, "⚠️ *GAS EMERGENCY!*\n\n🚨 Cannot turn fan OFF\n🌀 Fan forced ON for safety\n⚙️ Gas level: " + 
                            String(gasLevel), "Markdown");
          }
          return;
        }
        
        fanManualOverride = true; // Keep manual regime
        fanDesiredState = false;  // Fan is turn off
        lastFanToggleTime = millis();
        Serial.println("✅ Fan turned OFF from Node-RED (Manual mode)");
        
        feedFan.publish("OFF");
        
        if (chatIdFound) {
          bot.sendMessage(chatId, "✅ Fan turned OFF from Node-RED\n🔄 Mode: Manual control", "");
        }
      } 
      else if (fanCommand == "AUTO") {
        // Special command to return to automatic mode
        fanManualOverride = false; // Returning to automatic mode
        lastFanToggleTime = millis();
        Serial.println("🔄 Fan control switched to AUTO mode (gas sensor)");
        
        if (chatIdFound) {
          bot.sendMessage(chatId, "🔄 Fan control switched to AUTO mode\n⚙️ Gas sensor will control fan", "");
        }
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

  if (gasLevel > DANGEROUS_GAS) {
    gasAlert = true;
    digitalWrite(RED_LED, HIGH);
    if (millis() - lastGasAlertTime > ALERT_COOLDOWN) {
      String alertMsg = "🚨 *WARNING! Danger gas concentration level!* 🚨\n";
      alertMsg += "Gas level: " + String(gasLevel) + "\n";
      alertMsg += "Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "ON (forced)" : "ERROR!") + "\n";
      if (fanManualOverride) {
        alertMsg += "⚠️ Manual control overridden for safety!";
      }
      sendTelegramAlert(alertMsg);
      lastGasAlertTime = millis();
    }
  } else if (prevGasAlert && !gasEmergencyActive) {
    sendTelegramAlert("✅ Gas level returned to normal\nCurrent: " + String(gasLevel));
  }

  if (temperature < LOW_TEMP_THRESHOLD || temperature > HIGH_TEMP_THRESHOLD) {
    tempAlert = true;
    digitalWrite(YELLOW_LED, HIGH);
    if (millis() - lastTempAlertTime > ALERT_COOLDOWN) {
      sendTelegramAlert("🌡️ *Temperature out of range!* " + String(temperature) + "°C");
      lastTempAlertTime = millis();
    }
  } else if (prevTempAlert) {
    sendTelegramAlert("✅ Temperature returned to normal: " + String(temperature) + "°C");
  }

  if (humidity > HIGH_HUMIDITY_THRESHOLD) {
    humidityAlert = true;
    digitalWrite(GREEN_LED, HIGH);
    if (millis() - lastHumidityAlertTime > ALERT_COOLDOWN) {
      sendTelegramAlert("💧 *High humidity!* " + String(humidity) + "%");
      lastHumidityAlertTime = millis();
    }
  } else if (prevHumidityAlert) {
    sendTelegramAlert("✅ Humidity returned to normal: " + String(humidity) + "%");
  }

  if (motionDetected && !prevMotionState) {
    sendTelegramAlert("🚶 *Motion detected!*");
    lastMotionAlertTime = millis();
  } else if (!motionDetected && prevMotionState && (millis() - lastMotionAlertTime > 5000)) {
    sendTelegramAlert("✅ *Motion stopped*");
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

// ============= TELEGRAM BOT functions =============

void setupTelegram() {
  Serial.println("Setting up Telegram Bot...");
  
  Serial.println("🤖 Telegram Bot is ready!");
  Serial.println("📱 Send any message to the bot @Smart_AirGuard_bot");
  Serial.println("💬 For example: /start or Hello");
}

void handleTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String incoming_chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      
      Serial.print("📩 Telegram from ");
      Serial.print(from_name);
      Serial.print(" (ID: ");
      Serial.print(incoming_chat_id);
      Serial.print("): ");
      Serial.println(text);
      
      // Chat ID auto saving
      if (chatId == "") {
        chatId = incoming_chat_id;
        chatIdFound = true;
        Serial.print("✅ Chat ID is saved: ");
        Serial.println(chatId);
        
        // Send a welcome message
        String welcomeMsg = "🤖 *Smart AirGuard System is started!*\n\n";
        welcomeMsg += "👋 Hello, " + from_name + "!\n";
        welcomeMsg += "✅ Your Chat ID is saved: `" + chatId + "`\n";
        welcomeMsg += "📡 System IP: " + WiFi.localIP().toString() + "\n";
        welcomeMsg += "⏱️ Startup time: " + getUptime() + "\n\n";
        welcomeMsg += "📊 *Use the following commands:*\n";
        welcomeMsg += "/status - Current status\n";
        welcomeMsg += "/sensors - Sensors data\n";
        welcomeMsg += "/alerts - Active alerts\n";
        welcomeMsg += "/fan_on - Turn fan ON (manual)\n";
        welcomeMsg += "/fan_off - Turn fan OFF (manual)*\n";
        welcomeMsg += "/fan_auto - Auto mode (gas sensor)\n";
        welcomeMsg += "/help - Commands info\n\n";
        welcomeMsg += "*⚠️ Note: During gas emergencies, fan control will be restored after 10 seconds of normal gas levels";
        
        bot.sendMessage(chatId, welcomeMsg, "Markdown");
      }
      
      // Commands 
      if (text == "/start" || text == "/help") {
        String helpMsg = "🤖 *Monitoring System Commands*\n\n";
        helpMsg += "/status - Current system status\n";
        helpMsg += "/sensors - Sensors data\n";
        helpMsg += "/alerts - Active alerts\n";
        helpMsg += "/fan_on - Turn fan ON (manual mode)\n";
        helpMsg += "/fan_off - Turn fan OFF (manual mode)*\n";
        helpMsg += "/fan_auto - Switch to AUTO mode (gas sensor)\n";
        helpMsg += "/id - Show your Chat ID\n";
        helpMsg += "/help - This help message\n\n";
        helpMsg += "📊 *Automatic alerts:*\n";
        helpMsg += "• Gas level > 400 (HIGHEST priority)\n";
        helpMsg += "• Temperature outside 10-35°C\n";
        helpMsg += "• Humidity > 90%\n";
        helpMsg += "• Motion detected\n\n";
        helpMsg += "*⚠️ Safety features:*\n";
        helpMsg += "• During gas emergencies, fan turns ON automatically\n";
        helpMsg += "• Manual control restored after 10s of normal gas\n";
        helpMsg += "• Cannot turn fan OFF during gas emergency";
        
        bot.sendMessage(chatId, helpMsg, "Markdown");
      }
      else if (text == "/status") {
        sendStatus(chatId);
      }
      else if (text == "/sensors") {
        sendSensorData(chatId);
      }
      else if (text == "/alerts") {
        String alertsMsg = "🚨 *Active alerts*\n\n";
        
        if (gasLevel > DANGEROUS_GAS || tempAlert || humidityAlert) {
          if (gasLevel > DANGEROUS_GAS) {
            alertsMsg += "🔴 *GAS: DANGEROUS LEVEL!*\n";
            alertsMsg += "   Level: " + String(gasLevel) + "\n";
            alertsMsg += "   Threshold: " + String(DANGEROUS_GAS) + "\n";
            alertsMsg += "   Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "ON (forced)" : "ERROR!") + "\n\n";
          }
          if (tempAlert) {
            alertsMsg += "🟡 *TEMPERATURE OUT OF RANGE*\n";
            alertsMsg += "   Current: " + String(temperature, 1) + "°C\n";
            alertsMsg += "   Range: 10-35°C\n\n";
          }
          if (humidityAlert) {
            alertsMsg += "🟡 *HIGH HUMIDITY*\n";
            alertsMsg += "   Current: " + String(humidity, 1) + "%\n";
            alertsMsg += "   Threshold: 90%\n\n";
          }
        } else {
          alertsMsg += "✅ *All parameters are normal*\n";
          alertsMsg += "No active alerts";
        }
        
        if (motionDetected) {
          alertsMsg += "🚶 *Motion detected*\n";
          alertsMsg += "   Time since last: " + 
                      String((millis() - lastMotionTime) / 1000) + " seconds";
        }
        
        alertsMsg += "\n\n⚙️ *Fan control mode:* ";
        if (gasEmergencyActive) {
          alertsMsg += "GAS EMERGENCY (forced ON)\n";
          if (gasNormalizedTime > 0) {
            int secondsLeft = GAS_NORMAL_DELAY - (millis() - gasNormalizedTime);
            alertsMsg += "   Control restored in: " + String(max(0, secondsLeft / 1000)) + " seconds";
          }
        } else {
          alertsMsg += String(fanManualOverride ? "MANUAL" : "AUTO");
        }
        
        bot.sendMessage(chatId, alertsMsg, "Markdown");
      }
      else if (text == "/fan_on") {
        fanManualOverride = true;
        fanDesiredState = true;
        bot.sendMessage(chatId, "✅ *Fan turned ON*\n🔄 Mode: Manual control", "Markdown");
      }
      else if (text == "/fan_off") {
        // Проверяем газ перед выключением
        if (gasLevel > DANGEROUS_GAS) {
          bot.sendMessage(chatId, "⚠️ *GAS EMERGENCY!*\n\n🚨 Cannot turn fan OFF\n🌀 Fan must stay ON for safety\n⚙️ Gas level: " + 
                          String(gasLevel), "Markdown");
        } else {
          fanManualOverride = true;
          fanDesiredState = false;
          bot.sendMessage(chatId, "✅ *Fan turned OFF*\n🔄 Mode: Manual control", "Markdown");
        }
      }
      else if (text == "/fan_auto") {
        fanManualOverride = false;
        bot.sendMessage(chatId, "🔄 *Fan control switched to AUTO mode*\n⚙️ Gas sensor will control fan", "Markdown");
      }
      else if (text == "/id") {
        String idMsg = "📱 *Your Chat ID:*\n\n";
        idMsg += "```\n" + chatId + "\n```\n\n";
        idMsg += "💾 Save it for system configuration";
        bot.sendMessage(chatId, idMsg, "Markdown");
      }
      else {
        String reply = "Unknown command: " + text + "\n";
        reply += "Use /help for list of commands";
        bot.sendMessage(chatId, reply, "");
      }
    }
    
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void sendTelegramAlert(String message) {
  if (!chatIdFound) {
    return; // Do not send if Chat ID has not been found yet
  }
  
  String alertMsg = "⚠️ *SYSTEM ALERT* ⚠️\n\n";
  alertMsg += message;
  alertMsg += "\n\n🕐 Uptime: " + getUptime();
  alertMsg += "\n📍 System: Smart AirGuard";
  
  bot.sendMessage(chatId, alertMsg, "Markdown");
  Serial.println("📤 Telegram alert sent");
}

void sendStatus(String chat_id) {
  String statusMsg = "📊 *System status*\n\n";
  
  statusMsg += "⏱️ Uptime: " + getUptime() + "\n";
  statusMsg += "📶 Wi-Fi: " + String(WiFi.SSID()) + " (" + 
               String(WiFi.RSSI()) + " dBm)\n";
  statusMsg += "📡 IP: " + WiFi.localIP().toString() + "\n";
  statusMsg += "🔄 Last ThingSpeak update: " + 
               String((millis() - lastThingSpeakUpdate) / 1000) + " seconds ago\n";
  statusMsg += "📡 MQTT Status: " + String(mqtt.connected() ? "✅ Connected" : "❌ Disconnected") + "\n\n";
  
  statusMsg += "🚦 *States:*\n";
  statusMsg += "• Gas level: " + String(gasLevel) + " (" + 
               (gasLevel > DANGEROUS_GAS ? "🔴 DANGEROUS" : "✅ Ok") + ")\n";
  statusMsg += "• Temperature: " + String(temperature, 1) + "°C (" + 
               (tempAlert ? "🟡 Warning" : "✅ Ok") + ")\n";
  statusMsg += "• Humidity: " + String(humidity, 1) + "% (" + 
               (humidityAlert ? "🟢 High" : "✅ Ok") + ")\n";
  statusMsg += "• Motion: " + String(motionDetected ? "🔵 Detected" : "⚫ No") + "\n";
  statusMsg += "• Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "🌀 ON" : "⭕ OFF") + "\n";
  statusMsg += "• Fan mode: ";
  if (gasEmergencyActive) {
    statusMsg += "🔴 GAS EMERGENCY (forced ON)";
    if (gasNormalizedTime > 0) {
      int secondsLeft = GAS_NORMAL_DELAY - (millis() - gasNormalizedTime);
      statusMsg += "\n   Control restored in: " + String(max(0, secondsLeft / 1000)) + " seconds";
    }
  } else {
    statusMsg += String(fanManualOverride ? "🔄 MANUAL" : "⚙️ AUTO");
  }
  statusMsg += "\n\n";
  
  statusMsg += "📈 *Cloud services:*\n";
  statusMsg += "• ThingSpeak: Every " + String(THINGSPEAK_DELAY / 1000) + " seconds\n";
  statusMsg += "• Adafruit IO: On value change (or every 60 sec)\n";
  statusMsg += "• Telegram: Real-time alerts\n\n";
  
  statusMsg += "⚠️ *Safety note:* During gas emergencies (>400), fan turns ON automatically. Control restored 10 seconds after gas normalizes.";
  
  bot.sendMessage(chat_id, statusMsg, "Markdown");
}

void sendSensorData(String chat_id) {
  String sensorMsg = "📡 *Real-time Sensor Data*\n\n";
  
  sensorMsg += "🌡️ Temperature: *" + String(temperature, 1) + "°C*\n";
  sensorMsg += "💧 Humidity: *" + String(humidity, 1) + "%*\n";
  sensorMsg += "⚠️ Gas level: *" + String(gasLevel) + "*\n";
  sensorMsg += "🚶 Motion: *" + String(motionDetected ? "Yes" : "No") + "*\n";
  sensorMsg += "🌀 Fan: *" + String(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF") + "*\n";
  sensorMsg += "⚙️ Fan mode: *";
  if (gasEmergencyActive) {
    sensorMsg += "GAS EMERGENCY (forced ON)";
    if (gasNormalizedTime > 0) {
      int secondsLeft = GAS_NORMAL_DELAY - (millis() - gasNormalizedTime);
      sensorMsg += " - restoring in " + String(max(0, secondsLeft / 1000)) + "s";
    }
  } else {
    sensorMsg += String(fanManualOverride ? "Manual" : "Auto");
  }
  sensorMsg += "*\n\n";
  
  sensorMsg += "📊 *Thresholds:*\n";
  sensorMsg += "• Dangerous gas level: > " + String(DANGEROUS_GAS) + "\n";
  sensorMsg += "• Temperature: " + String(LOW_TEMP_THRESHOLD) + 
               " - " + String(HIGH_TEMP_THRESHOLD) + "°C\n";
  sensorMsg += "• Humidity: < " + String(HIGH_HUMIDITY_THRESHOLD) + "%\n\n";
  
  // Emojis for visualization
  String gasStatus = gasLevel > DANGEROUS_GAS ? "🔴 DANGEROUS" : 
                    (gasLevel > 200 ? "🟡 ELEVATED" : "✅ Ok");
  String tempStatus = tempAlert ? "🟡 WARNING" : "✅  Ok";
  String humidStatus = humidityAlert ? "🟢 HIGH" : "✅ Ok";
  
  sensorMsg += "🎯 *Current status:*\n";
  sensorMsg += gasStatus + " (" + String(gasLevel) + ")\n";
  sensorMsg += tempStatus + " (" + String(temperature, 1) + "°C)\n";
  sensorMsg += humidStatus + " (" + String(humidity, 1) + "%)";
  
  bot.sendMessage(chat_id, sensorMsg, "Markdown");
}

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
