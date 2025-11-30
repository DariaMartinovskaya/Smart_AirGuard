#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ИСПРАВЛЕННЫЕ пины - БЕЗОПАСНЫЕ
#define DHTPIN 14
#define BUZZER_PIN 13     
#define RED_LED 21
#define YELLOW_LED 19
#define GREEN_LED 18
#define MQ135_PIN 34      
#define RELAY_PIN 26       

// Константы
#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Пороговые значения
#define DANGEROUS_GAS 400
#define LOW_TEMP_THRESHOLD 10
#define HIGH_TEMP_THRESHOLD 35
#define HIGH_HUMIDITY_THRESHOLD 90

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

// Прототипы функций
void readSensors();
void checkConditions();
void displayData();
void toneAlert();

void setup() {
  Serial.begin(115200);
  
  // ✅ ЯВНО инициализируем I2C с указанием пинов
  Wire.begin(23, 22); // SDA=23, SCL=22
  
  // Инициализация пинов
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  
  // Выключить все при старте
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  
  // Инициализация датчиков
  dht.begin();
  
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
  display.display();
  delay(2000);
  
  Serial.println("System initialized");
}

void loop() {
  readSensors();
  checkConditions();
  displayData();
  delay(2000);
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
  Serial.print("%, Gas: "); Serial.println(gasLevel);
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
  
  display.println("-------------------");
  
  if (gasAlert) {
    display.setTextColor(SSD1306_WHITE);
    display.println("! GAS DANGER !");
  }
  if (tempAlert) {
    display.setTextColor(SSD1306_WHITE);
    display.println("! TEMP ALERT !");
  }
  if (humidityAlert) {
    display.setTextColor(SSD1306_WHITE);
    display.println("! HUMID HIGH !");
  }
  
  display.setTextColor(SSD1306_WHITE);
  display.print("Fan: ");
  display.println(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF");
  
  display.display();
  Serial.println("✅ Data sent to OLED"); // Отладочное сообщение
}
