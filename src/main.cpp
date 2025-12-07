#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ---------------- WIFI + THINGSPEAK -----------------
const char *ssid = "Alex";
const char *pass = "Sacha3232";
String apiKey = "6QOIQZ7YFHAG6231";  
const char* server = "api.thingspeak.com";

WiFiClient client;
HTTPClient http;

unsigned long lastThingSpeakUpdate = 0;
const unsigned long THINGSPEAK_DELAY = 15000; // 15 секунд между отправками

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

// Константы
#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Пороговые значения
#define DANGEROUS_GAS 400
#define LOW_TEMP_THRESHOLD 10
#define HIGH_TEMP_THRESHOLD 35
#define HIGH_HUMIDITY_THRESHOLD 90

// Тайминг для PIR
#define PIR_DEBOUNCE_TIME 2000  // Время стабилизации датчика (2 секунды)
#define MOTION_TIMEOUT 10000    // Таймаут обнаружения движения (10 секунд)
#define BLINK_INTERVAL 500      // Интервал мигания синего LED при движении

// Объекты
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// Переменные
float temperature = 0;
float humidity = 0;
int gasLevel = 0;
bool gasAlert = false;
bool tempAlert = false;
bool humidityAlert = false;
bool motionDetected = false;    // состояние датчика движения
unsigned long lastMotionTime = 0; // время последнего обнаружения движения
unsigned long pirReadyTime = 0;   // время готовности PIR
unsigned long lastBlinkTime = 0;  // время последнего мигания
bool blueLedState = false;        // состояние синего LED

// Прототипы функций
void readSensors();
void sendToThingSpeak();
void checkMotion();
void checkConditions();
void displayData();
void updateRGBLed();
void setRGBColor(bool red, bool green, bool blue);
void toneAlert();

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
  
  // Инициализация пинов
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  
  // ✅ ДОБАВЛЕНО: инициализация пинов RGB LED
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
  
  // Выключить все при старте
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
  // ✅ ДОБАВЛЕНО: выключить RGB LED
  setRGBColor(false, false, false);
  
  // Инициализация датчиков
  dht.begin();
  
  // Инициализация времени PIR
  pirReadyTime = millis() + PIR_DEBOUNCE_TIME;
  Serial.print("PIR initializing (wait ");
  Serial.print(PIR_DEBOUNCE_TIME / 1000);
  Serial.println(" seconds)...");
  
  // Инициализация дисплея с проверкой
  Serial.println("Initializing OLED...");
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED NOT FOUND! Check:");
    Serial.println("- SDA -> GPIO 23");
    Serial.println("- SCL -> GPIO 22"); 
    Serial.println("- VCC -> 3.3V");
    Serial.println("- GND -> GND");
    while(1); // Остановить если OLED не найден
  }
  
  Serial.println("✅ OLED initialized!");
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("System Starting...");
  display.println("PIR initializing");
  display.display();
  delay(PIR_DEBOUNCE_TIME); // Ждем стабилизации PIR
  
  // ✅ ДОБАВЛЕНО: тест RGB LED
  Serial.println("Testing RGB LED...");
  setRGBColor(true, false, false); // Красный
  delay(300);
  setRGBColor(false, true, false); // Зеленый
  delay(300);
  setRGBColor(false, false, true); // Синий
  delay(300);
  setRGBColor(false, false, false); // Выключить
  delay(300);
  
  // ✅ ГАРАНТИРОВАННОЕ ВЫКЛЮЧЕНИЕ СИНЕГО LED
  blueLedState = false;
  setRGBColor(false, false, false);
  Serial.println("Blue LED: OFF (default state)");
  
  Serial.println("System initialized");
  Serial.println("Ready to send data to ThingSpeak");
}

void loop() {
  readSensors();
  checkMotion();
  updateRGBLed();    // ✅ ДОБАВЛЕНО: обновление RGB LED
  checkConditions();
  displayData();
  
  // Отправка данных в ThingSpeak каждые 15 секунд
  if (millis() - lastThingSpeakUpdate >= THINGSPEAK_DELAY) {
    sendToThingSpeak();
    lastThingSpeakUpdate = millis();
  }
  
  delay(100); // Уменьшили задержку для более плавного мигания
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

// ✅ ДОБАВЛЕНА ФУНКЦИЯ: отправка данных в ThingSpeak
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    // Правильный URL для ThingSpeak
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
    Serial.println(url);  // ← ДОБАВЬТЕ для отладки
    
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.print("ThingSpeak HTTP code: ");
      Serial.println(httpCode);
      
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("Response: ");
        Serial.println(payload);
        
        // ThingSpeak возвращает номер записи (entry_id)
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
  // Проверяем, готов ли PIR (прошло ли время инициализации)
  if (millis() < pirReadyTime) {
    return; // PIR еще не готов
  }
  
  int pirState = digitalRead(PIR_PIN);
  
  if (pirState == HIGH) {
    // Движение обнаружено
    if (!motionDetected) {
      motionDetected = true;
      lastMotionTime = millis();
      lastBlinkTime = millis(); // Сбрасываем таймер мигания
      Serial.println("Motion detected!");
    } else {
      lastMotionTime = millis(); // Обновляем время последнего движения
    }
  } else {
    // Нет движения в данный момент
    // Проверяем таймаут движения
    if (motionDetected && (millis() - lastMotionTime > MOTION_TIMEOUT)) {
      motionDetected = false;
      blueLedState = false; // ✅ ЯВНО сбрасываем состояние
      setRGBColor(false, false, false); // ✅ Гарантированно выключаем синий
      Serial.println("Motion timeout - Blue LED turned OFF");
    }
  }
}

void updateRGBLed() {
  if (motionDetected) {
    // ✅ Мигание синим при обнаружении движения
    if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
      blueLedState = !blueLedState;
      setRGBColor(false, false, blueLedState);
      lastBlinkTime = millis();
    }
  } else {
    // ✅ Если движения нет - ВСЕГДА выключаем синий LED
    // Это гарантирует, что синий LED не горит когда не должен
    if (blueLedState) {
      blueLedState = false;
      setRGBColor(false, false, false);
    }
    // ✅ Дополнительная защита: периодически проверяем и выключаем
    static unsigned long lastCheckTime = 0;
    if (millis() - lastCheckTime > 1000) {
      // Раз в секунду проверяем и гарантируем выключение
      digitalWrite(RGB_BLUE, LOW);
      lastCheckTime = millis();
    }
  }
}

// ✅ ДОБАВЛЕНО: функция установки цвета RGB LED
void setRGBColor(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE, blue ? HIGH : LOW);
  
  // Отладочный вывод
  if (blue) {
    Serial.println("setRGBColor: Blue ON");
  }
}

void checkConditions() {
  gasAlert = false;
  tempAlert = false;
  humidityAlert = false;
  
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
  // Приоритет: газ > температура > влажность
  if (gasLevel > DANGEROUS_GAS) {
    gasAlert = true;
    digitalWrite(RED_LED, HIGH);
    digitalWrite(RELAY_PIN, LOW);
    toneAlert();
    Serial.println("DANGER! High gas concentration!");
  }
  
  if (temperature < LOW_TEMP_THRESHOLD || temperature > HIGH_TEMP_THRESHOLD) {
    tempAlert = true;
    digitalWrite(YELLOW_LED, HIGH);
    Serial.println("Temperature alert!");
  }
  
  if (humidity > HIGH_HUMIDITY_THRESHOLD) {
    humidityAlert = true;
    digitalWrite(GREEN_LED, HIGH);
    Serial.println("High humidity alert!");
  }
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
  
  // Отображение предупреждений
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
  
  // ✅ ИЗМЕНЕНО: вместо "MOTION ACTIVE" показываем состояние вентилятора
  // Вентилятор включается только при обнаружении газа
  
  display.print("Fan: ");
  display.println(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF");
  
  // Отображение времени с последнего движения
  if (motionDetected) {
    display.print("Last motion: ");
    display.print((millis() - lastMotionTime) / 1000);
    display.println("s ago");
  }
  
  // ✅ ДОБАВЛЕНО: индикация состояния синего LED
  display.print("Blue LED: ");
  display.println(blueLedState ? "ON" : "OFF");
  
  // ✅ ДОБАВЛЕНО: показываем время до следующей отправки в ThingSpeak
  display.print("TS: ");
  display.print(max(0, (int)(THINGSPEAK_DELAY - (millis() - lastThingSpeakUpdate)) / 1000));
  display.println("s");
  
  display.display();
}
