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

// ---------------- WIFI + THINGSPEAK -----------------
const char *ssid = "Alex";
const char *pass = "Sacha3232";
String apiKey = "6QOIQZ7YFHAG6231";  
const char* server = "api.thingspeak.com";

// ---------------- TELEGRAM BOT -----------------
#define BOT_TOKEN "8159417350:AAFVaCa_djh0A81HtL3QTT7Ww9rcLwBU4Yg"  

String chatId = "";  
bool chatIdFound = false;

WiFiClient client;
WiFiClientSecure secured_client;
HTTPClient http;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

unsigned long lastThingSpeakUpdate = 0;
const unsigned long THINGSPEAK_DELAY = 15000;
unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_DELAY = 1000;

unsigned long lastGasAlertTime = 0;
unsigned long lastTempAlertTime = 0;
unsigned long lastHumidityAlertTime = 0;
unsigned long lastMotionAlertTime = 0;
const unsigned long ALERT_COOLDOWN = 30000;

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
#define RGB_BLUE 33       // Blue channel (for motion)

#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// 
#define DANGEROUS_GAS 400
#define LOW_TEMP_THRESHOLD 10
#define HIGH_TEMP_THRESHOLD 35
#define HIGH_HUMIDITY_THRESHOLD 90

// Timing for PIR
#define PIR_DEBOUNCE_TIME 2000  // Sensor stabilisation time (2 secs)
#define MOTION_TIMEOUT 10000    // Motion detection timeout (10 seconds)
#define BLINK_INTERVAL 500      // Blue LED blinking interval during motion

// Objects
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Variables
float temperature = 0;
float humidity = 0;
int gasLevel = 0;
bool gasAlert = false;
bool tempAlert = false;
bool humidityAlert = false;
bool motionDetected = false;    // Motion sensor state
unsigned long lastMotionTime = 0; // Time of last detected motion
unsigned long pirReadyTime = 0;   // PIR readiness time
unsigned long lastBlinkTime = 0;  // Time of last LED blink
bool blueLedState = false;        // Blue LED state

// Flags for tracking changes
bool lastGasAlert = false;
bool lastTempAlert = false;
bool lastHumidityAlert = false;
bool lastMotionState = false;  

// Function prototypes
void readSensors();
void sendToThingSpeak();
void checkMotion();
void checkConditions();
void displayData();
void updateRGBLed();
void setRGBColor(bool red, bool green, bool blue);
void toneAlert();

// Telegram function prototypes
void setupTelegram();
void handleTelegramMessages();
void sendTelegramAlert(String message);
void sendStatus(String chat_id);
void sendSensorData(String chat_id);
String getUptime();

void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // I2C
  Wire.begin(23, 22);
  
  // Pins initialization
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  
  // RGB LED pins initialization
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
  setRGBColor(false, false, false);

  dht.begin();
  pirReadyTime = millis() + PIR_DEBOUNCE_TIME;

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR!");
    while (1);
  }
  
  // Turn everything off at startup
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
  // Turn RGB LED on
  setRGBColor(false, false, false);
  
  // Sensors initialization
  dht.begin();
  
  // PIR timing initialization
  pirReadyTime = millis() + PIR_DEBOUNCE_TIME;
  Serial.print("PIR initializing (wait ");
  Serial.print(PIR_DEBOUNCE_TIME / 1000);
  Serial.println(" seconds)...");
  
  // Display initialization with check
  Serial.println("Initializing OLED...");
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED NOT FOUND! Check:");
    Serial.println("- SDA -> GPIO 23");
    Serial.println("- SCL -> GPIO 22"); 
    Serial.println("- VCC -> 3.3V");
    Serial.println("- GND -> GND");
    while(1); // Stop if OLED is not found
  }
  
  Serial.println("✅ OLED initialized!");
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("System Starting...");
  display.println("PIR initializing");
  display.display();
  delay(PIR_DEBOUNCE_TIME); // Waiting for PIR is stabilized
  
  // RGB LED Test
  Serial.println("Testing RGB LED...");
  setRGBColor(true, false, false); // Red
  delay(300);
  setRGBColor(false, true, false); // Green
  delay(300);
  setRGBColor(false, false, true); // Blue
  delay(300);
  setRGBColor(false, false, false); // Turn off
  delay(300);
  
  // FORCED BLUE LED TURN-OFF
  blueLedState = false;
  setRGBColor(false, false, false);
  Serial.println("Blue LED: OFF (default state)");
  
  Serial.println("System initialized");
  Serial.println("Ready to send data to ThingSpeak");

   // Telegram bot set up
  setupTelegram();
}

void loop() {
  readSensors();
  checkMotion();
  updateRGBLed();    
  checkConditions();
  displayData();
  
  // Sending data to ThingSpeak every 15 seconds
  if (millis() - lastThingSpeakUpdate >= THINGSPEAK_DELAY) {
    sendToThingSpeak();
    lastThingSpeakUpdate = millis();
  }
  
  // Checking Telegram messages
  if (millis() - lastTelegramCheck >= TELEGRAM_DELAY) {
    handleTelegramMessages();
    lastTelegramCheck = millis();
  }
  
  delay(100); 
}

void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  gasLevel = analogRead(MQ135_PIN);
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error reading DHT22!");
    temperature = 0;
    humidity = 0;
  }
  
  Serial.print("Temp: "); Serial.print(temperature);
  Serial.print("C, Humidity: "); Serial.print(humidity);
  Serial.print("%, Gas: "); Serial.print(gasLevel);
  Serial.print(", Motion: "); Serial.println(motionDetected ? "YES" : "NO");
  Serial.print("Blue LED state: "); Serial.println(blueLedState ? "ON" : "OFF");
}

// Sending data to ThingSpeak
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
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
    
    Serial.print("Sending to ThingSpeak: ");
    Serial.println(url);  // ← To be added for debugging
    
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.print("ThingSpeak HTTP code: ");
      Serial.println(httpCode);
      
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("Response: ");
        Serial.println(payload);
        
        // ThingSpeak return item number (entry_id)
        if (payload.toInt() > 0) {
          Serial.println("✅ Data sent successfully!");
        } else {
          Serial.println("❌ ThingSpeak returned 0 - check API key!");
        }
      }
    } else {
      Serial.print("❌ Error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    
    http.end();
  } else {
    Serial.println("❌ WiFi disconnected!");
  }
}

void checkMotion() {
  // Check if PIR is ready 
  if (millis() < pirReadyTime) {
    return; // PIR is not ready yet
  }
  
  int pirState = digitalRead(PIR_PIN);
  
  if (pirState == HIGH) {
    // Motion detected
    if (!motionDetected) {
      motionDetected = true;
      lastMotionTime = millis();
      lastBlinkTime = millis(); // Reset the blink timer
      Serial.println("Motion detected!");
    } else {
      lastMotionTime = millis(); // Update the time of the last motion
    }
  } else {
    // No motion at the moment
    // Check motion timeout
    if (motionDetected && (millis() - lastMotionTime > MOTION_TIMEOUT)) {
      motionDetected = false;
      blueLedState = false; // Explicitly reset the state
      setRGBColor(false, false, false); // Ensure the blue LED is turned off
      Serial.println("Motion timeout - Blue LED turned OFF");
    }
  }
}

void updateRGBLed() {
  if (motionDetected) {
    // Blink blue LED when motion is detected
    if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
      blueLedState = !blueLedState;
      setRGBColor(false, false, blueLedState);
      lastBlinkTime = millis();
    }
  } else {
    // If no motion - ALWAYS turn off the blue LED
    if (blueLedState) {
      blueLedState = false;
      setRGBColor(false, false, false);
    }
    // Extra protection: periodically check and turn off
    static unsigned long lastCheckTime = 0;
    if (millis() - lastCheckTime > 1000) {
      // Check once per second and ensure it is turned off
      digitalWrite(RGB_BLUE, LOW);
      lastCheckTime = millis();
    }
  }
}

// Function to set RGB LED color
void setRGBColor(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE, blue ? HIGH : LOW);
  
  // Debug output
  if (blue) {
    Serial.println("setRGBColor: Blue ON");
  }
}

void checkConditions() {
  // Save previous states for comparison
  lastGasAlert = gasAlert;
  lastTempAlert = tempAlert;
  lastHumidityAlert = humidityAlert;

  gasAlert = false;
  tempAlert = false;
  humidityAlert = false;
  
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
  // Priority: gas > temperature > humidity
  if (gasLevel > DANGEROUS_GAS) {
    gasAlert = true;
    digitalWrite(RED_LED, HIGH);
    digitalWrite(RELAY_PIN, LOW);
    toneAlert();
    Serial.println("DANGER! High gas concentration!");

    // Send Telegram alert при каждом срабатывании (с учетом кулдауна)
    if (millis() - lastGasAlertTime > ALERT_COOLDOWN) {
      sendTelegramAlert("🚨 *WARNING! Danger gas concentration level!* 🚨\n\n" +
                       String("Gas level: ") + gasLevel + "\n" +
                       "Danger threshold: " + String(DANGEROUS_GAS) + "\n" +
                       "Fan: Turned ON automatically");
      lastGasAlertTime = millis();
    }
  } else if (lastGasAlert) {
    // Газ вернулся в норму
    sendTelegramAlert("✅ *Gas level returned to normal*\n\n" +
                     String("Current gas level: ") + gasLevel + "\n" +
                     "Fan: Turned OFF");
    lastGasAlertTime = millis();
  }
  
  if (temperature < LOW_TEMP_THRESHOLD || temperature > HIGH_TEMP_THRESHOLD) {
    tempAlert = true;
    digitalWrite(YELLOW_LED, HIGH);
    Serial.println("Temperature alert!");

    // Send Telegram alert при каждом срабатывании
    if (millis() - lastTempAlertTime > ALERT_COOLDOWN) {
      sendTelegramAlert("🌡️ *Temperature out of normal range!*\n\n" +
                       String("Current temperature: ") + temperature + "°C\n" +
                       "Normal range: " + String(LOW_TEMP_THRESHOLD) + 
                       " - " + String(HIGH_TEMP_THRESHOLD) + "°C");
      lastTempAlertTime = millis();
    }
  } else if (lastTempAlert) {
    // Температура вернулась в норму
    sendTelegramAlert("✅ *Temperature returned to normal*\n\n" +
                     String("Current temperature: ") + temperature + "°C\n" +
                     "Normal range: " + String(LOW_TEMP_THRESHOLD) + 
                     " - " + String(HIGH_TEMP_THRESHOLD) + "°C");
    lastTempAlertTime = millis();
  }
  
  if (humidity > HIGH_HUMIDITY_THRESHOLD) {
    humidityAlert = true;
    digitalWrite(GREEN_LED, HIGH);
    Serial.println("High humidity alert!");
  
    // Send Telegram alert при каждом срабатывании
    if (millis() - lastHumidityAlertTime > ALERT_COOLDOWN) {
      sendTelegramAlert("💧 *High humidity!*\n\n" +
                       String("Current humidity: ") + humidity + "%\n" +
                       "Threshold: " + String(HIGH_HUMIDITY_THRESHOLD) + "%");
      lastHumidityAlertTime = millis();
    }
  } else if (lastHumidityAlert) {
    // Влажность вернулась в норму
    sendTelegramAlert("✅ *Humidity returned to normal*\n\n" +
                     String("Current humidity: ") + humidity + "%\n" +
                     "Threshold: " + String(HIGH_HUMIDITY_THRESHOLD) + "%");
    lastHumidityAlertTime = millis();
  }

  // Motion detected alert
  if (motionDetected && !lastMotionState) {
    // Отправляем при обнаружении движения (без кулдауна для движения)
    sendTelegramAlert("🚶 *Motion detected!*\n\n" +
                     String("Time: ") + String(millis() / 1000) + " sec\n" +
                     "PIR Sensor: ACTIVE");
    lastMotionAlertTime = millis();
  }
  
  // Motion stopped alert (опционально)
  if (!motionDetected && lastMotionState && (millis() - lastMotionAlertTime > 5000)) {
    // Отправляем когда движение прекратилось (через 5 секунд)
    sendTelegramAlert("✅ *Motion stopped*\n\n" +
                     String("Motion duration: ") + 
                     String((millis() - lastMotionAlertTime) / 1000) + " seconds\n" +
                     "PIR Sensor: INACTIVE");
  }
  
  lastMotionState = motionDetected;
}

void toneAlert() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void displayData() {
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.setTextSize(1);
  display.println("ENVIRONMENT MONITOR");
  display.println("-------------------");
  
  display.print("Temp: ");
  display.print(temperature, 1);
  display.println(" C");
  
  display.print("Humidity: ");
  display.print(humidity, 1);
  display.println(" %");
  
  display.print("Gas Level: ");
  display.println(gasLevel);
  
  display.print("Motion: ");
  display.println(motionDetected ? "DETECTED" : "NONE");
  
  display.println("-------------------");
  
  // Alerts display
  if (gasAlert) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.println("! GAS DANGER !");
  }
  if (tempAlert) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.println("! TEMP ALERT !");
  }
  if (humidityAlert) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.println("! HUMID HIGH !");
  }
  
  // Fan turns on only when gas is detected
  
  display.print("Fan: ");
  display.println(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF");
  
  // Display time since last motion
  if (motionDetected) {
    display.print("Last motion: ");
    display.print((millis() - lastMotionTime) / 1000);
    display.println("s ago");
  }
  
  // Blue LED status indication
  display.print("Blue LED: ");
  display.println(blueLedState ? "ON" : "OFF");
  
  // Display time until next ThingSpeak update
  display.print("TS: ");
  display.print(max(0, (int)(THINGSPEAK_DELAY - (millis() - lastThingSpeakUpdate)) / 1000));
  display.println("s");
  
  // Display time until next Telegram check
  display.print("TG: ");
  display.print(max(0, (int)(TELEGRAM_DELAY - (millis() - lastTelegramCheck)) / 1000));
  display.println("s");
  
  display.display();
}

// ============= TELEGRAM BOT functions =============

void setupTelegram() {
  Serial.println("Setting up Telegram Bot...");
  
  secured_client.setInsecure();
  
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
        welcomeMsg += "/help - Commands info";
        
        bot.sendMessage(chatId, welcomeMsg, "Markdown");
      }
      
      // Commands 
      if (text == "/start" || text == "/help") {
        String helpMsg = "🤖 *Monitoring System Commands*\n\n";
        helpMsg += "/status - Current system status\n";
        helpMsg += "/sensors - Sensors data\n";
        helpMsg += "/alerts - Active alerts\n";
        helpMsg += "/id - Show your Chat ID\n";
        helpMsg += "/help - This help message\n\n";
        helpMsg += "📊 *Automatic alerts:*\n";
        helpMsg += "• Gas level > 400\n";
        helpMsg += "• Temperature outside 10-35°C\n";
        helpMsg += "• Humidity > 90%\n";
        helpMsg += "• Motion detected";
        
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
        
        if (gasAlert || tempAlert || humidityAlert) {
          if (gasAlert) {
            alertsMsg += "🔴 *GAS: DANGEROUS LEVEL!*\n";
            alertsMsg += "   Level: " + String(gasLevel) + "\n";
            alertsMsg += "   Threshold: " + String(DANGEROUS_GAS) + "\n";
            alertsMsg += "   Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF") + "\n\n";
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
                      String((millis() - lastMotionTime) / 1000) + " сек";
        }
        
        bot.sendMessage(chatId, alertsMsg, "Markdown");
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
  alertMsg += "\n\n🕐 Time: " + String(millis() / 1000) + " сек";
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
               String((millis() - lastThingSpeakUpdate) / 1000) + " сек назад\n\n";
  
  statusMsg += "🚦 *States:*\n";
  statusMsg += "• Gas: " + String(gasAlert ? "🔴 DANGEROUS" : "✅ Ok") + "\n";
  statusMsg += "• Temperature: " + String(tempAlert ? "🟡 Warning" : "✅ Ok") + "\n";
  statusMsg += "• Humidity: " + String(humidityAlert ? "🟢 High" : "✅ Ok") + "\n";
  statusMsg += "• Motion: " + String(motionDetected ? "🔵 Detected" : "⚫ Нет") + "\n";
  statusMsg += "• Fan: " + String(digitalRead(RELAY_PIN) == LOW ? "🌀 ON" : "⭕ OFF") + "\n\n";
  
  statusMsg += "📈 *ThingSpeak:*\n";
  statusMsg += "• Sending every " + String(THINGSPEAK_DELAY / 1000) + " seconds\n";
  statusMsg += "• API Key: " + apiKey.substring(0, 8) + "...";
  
  bot.sendMessage(chat_id, statusMsg, "Markdown");
}

void sendSensorData(String chat_id) {
  String sensorMsg = "📡 *Real-time Sensor Data*\n\n";
  
  sensorMsg += "🌡️ Temperature: *" + String(temperature, 1) + "°C*\n";
  sensorMsg += "💧 Humidity: *" + String(humidity, 1) + "%*\n";
  sensorMsg += "⚠️ Gas level: *" + String(gasLevel) + "*\n";
  sensorMsg += "🚶 Motion: *" + String(motionDetected ? "Yes" : "No") + "*\n\n";
  
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
    return String(days) + "д " + String(hours) + "ч " + String(minutes) + "м";
  } else if (hours > 0) {
    return String(hours) + "ч " + String(minutes) + "м " + String(seconds) + "с";
  } else if (minutes > 0) {
    return String(minutes) + "м " + String(seconds) + "с";
  } else {
    return String(seconds) + "с";
  }
}
