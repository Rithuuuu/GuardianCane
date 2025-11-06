#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <MPU6050.h>

// ==== Pin Definitions ====
#define DHTPIN 8
#define DHTTYPE DHT22
#define TRIG_PIN 9
#define ECHO_PIN 10
#define BUZZER_PIN 6
#define LED_RED 7
#define BUTTON_PIN 2
#define LDR_PIN A0

// ==== OLED Display ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==== Sensors ====
DHT dht(DHTPIN, DHTTYPE);
MPU6050 mpu;

// ==== Thresholds ====
#define DISTANCE_THRESHOLD 30  // cm
#define TEMP_MIN 0             // °C
#define TEMP_MAX 45            // °C
#define LIGHT_THRESHOLD 400    // 0–1023
#define HUMIDITY_MIN 30
#define HUMIDITY_MAX 60
#define ACCEL_THRESHOLD 15000  // motion sensitivity threshold (raw units)

// ==== Timing ====
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 500; // 0.5s
bool buttonPressed = false;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialize sensors
  dht.begin();
  mpu.initialize();

  // Pin setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed!"));
    while (true);
  }

  // Welcome Screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 10);
  display.println(F("SMART CANE v3.0"));
  display.setCursor(20, 25);
  display.println(F("Initializing..."));
  display.display();
  delay(2500);

  display.clearDisplay();
  display.display(); // turn OLED off (idle)

  // MPU6050 connection test
  if (mpu.testConnection()) {
    Serial.println(F("MPU6050 connected successfully!"));
  } else {
    Serial.println(F("MPU6050 connection failed!"));
  }

  Serial.println(F("Smart Cane Ready."));
  Serial.println(F("Press button or wait for alert."));
  Serial.println(F("----------------------------------"));
}

void loop() {
  // Button trigger
  if (digitalRead(BUTTON_PIN) == LOW) {
    buttonPressed = true;
    delay(100); // debounce
  }

  // Periodically check thresholds
  bool checkNow = (millis() - lastCheck > CHECK_INTERVAL);
  if (!checkNow && !buttonPressed) return;
  lastCheck = millis();

  // Read sensors
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float distance = getDistance();
  int lightLevel = analogRead(LDR_PIN);

  // Read acceleration
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  long totalAccel = abs(ax) + abs(ay) + abs(az);

  // Determine alerts
  bool distanceAlert = (distance > 0 && distance < DISTANCE_THRESHOLD);
  bool humidityAlert = (humidity < HUMIDITY_MIN || humidity > HUMIDITY_MAX);
  bool tempAlert = (!isnan(temperature) && (temperature < TEMP_MIN || temperature > TEMP_MAX));
  bool lightAlert = (lightLevel < LIGHT_THRESHOLD);
  bool motionAlert = (totalAccel > ACCEL_THRESHOLD);

  bool alertTriggered = buttonPressed || distanceAlert || tempAlert || lightAlert || humidityAlert || motionAlert;

  if (alertTriggered) {
    // Activate buzzer + LED
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER_PIN, 1500, 300);

    // OLED display alert
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("SMART CANE ALERT"));
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 15);
    display.print(F("Temp: "));
    display.print(temperature);
    display.println(F(" C"));
    display.setCursor(0, 25);
    display.print(F("Hum:  "));
    display.print(humidity);
    display.println(F(" %"));
    display.setCursor(0, 35);
    display.print(F("Dist: "));
    display.print(distance);
    display.println(F(" cm"));
    display.setCursor(0, 45);
    display.print(F("Light: "));
    display.print(lightLevel);
    display.display();

    // Serial output
    Serial.println(F("===== SMART CANE ALERT ====="));
    if (buttonPressed) Serial.println(F("🔘 Manual Button Triggered"));
    if (distanceAlert) Serial.println(F("⚠️ Obstacle detected nearby!"));
    if (tempAlert) Serial.println(F("⚠️ Temperature out of range!"));
    if (lightAlert) Serial.println(F("⚠️ Low light detected!"));
    if (humidityAlert) Serial.println(F("⚠️ Humidity uncomfortable!"));
    if (motionAlert) Serial.println(F("⚠️ Fall detected!"));

    Serial.print(F("Temperature: ")); Serial.print(temperature); Serial.println(F(" °C"));
    Serial.print(F("Humidity: ")); Serial.print(humidity); Serial.println(F(" %"));
    Serial.print(F("Distance: ")); Serial.print(distance); Serial.println(F(" cm"));
    Serial.print(F("Light Level: ")); Serial.println(lightLevel);
    Serial.print(F("Accel Sum: ")); Serial.println(totalAccel);
    Serial.println(F("============================\n"));

    delay(2000);

    // Turn off alerts after display
    noTone(BUZZER_PIN);
    digitalWrite(LED_RED, LOW);
    display.clearDisplay();
    display.display(); // turn OLED off again
  }

  // Reset button flag
  buttonPressed = false;
}

// ==== Helper: Measure Distance ====
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1; // timeout
  return duration * 0.034 / 2;
}