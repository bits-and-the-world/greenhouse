#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "DHT.h"

// pin settings
int soilMoistureSensorPin = A0;
int tempAndAirMoistureSensorPin = 15;

// Cloud Settings
String awsBaseUrl = "";
String greenhouseEndpoint = "/greenhouse";

// Time settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;  // Work with UTC
const int daylightOffset_sec = 3600;

// Sensors
DHT dht(tempAndAirMoistureSensorPin, DHT11);
int soilMoistureSensorValue;
int airMoistureSensorValue;
int temperatureSensorValue;

// Config
bool isDebugEnabled = true;
unsigned long checkInterval = 30000;
bool testMode = true;

boolean fansRunning = false;
boolean waterPumpRunning = false;
boolean waterAtomizerRunning = false;

unsigned long lastCheck = millis();


// ---- Setup
void setup() {

  initSerial();
  connectToWiFi();
  initInternalTime();
}

// ---- Main Loop
void loop() {
  if (sensorsNeedToBeChecked()) {
    checkAndAdjustTemperature();
    checkAndAdjustAirMoisture();
    checkSoilMoistureAndDoWatering();
  }
  /*
   * Erst Belüften (10 - 15°C sind optimal) - wir versuchen mal 20° zu halten
   * Dann Luftfeuchtigkeit anpassen
   * Dann bewässern
   */
  checkWiFiStatus();
  delay(30000);
}

// ---- Functions

void checkAndAdjustTemperature() {
  temperatureSensorValue = dht.readTemperature();
  Serial.println(temperatureSensorValue);
  sendDataToAWS("temperature", "003", String(temperatureSensorValue));
  // Adjust temperature by running fans
  if (temperatureSensorValue > 20 && !fansRunning) {
    //start Fans
    fansRunning = true;
  } else {
    // stop fans
    fansRunning = false;
  }
}

void checkSoilMoistureAndDoWatering() {
  soilMoistureSensorValue = analogRead(soilMoistureSensorPin);  // 382 = sensor is in water, 650 = dry, 450 = soakin wet
  Serial.println(soilMoistureSensorValue);
  sendDataToAWS("soil_moisture", "001", String(soilMoistureSensorValue));
  if (soilMoistureSensorValue < 550 && !waterPumpRunning) {
    // start watering pump
    waterPumpRunning = true;
  } else {
    waterPumpRunning = false;
    // stop waterpump
  }
}

void checkAndAdjustAirMoisture() {
  airMoistureSensorValue = dht.readHumidity();
  Serial.println(airMoistureSensorValue);
  // we need at least 70%
  if (airMoistureSensorValue <= 70) {
    // start water atomizer
  } else if (airMoistureSensorValue >= 90) {
    // stop water atomizer
  }

  sendDataToAWS("air_moisture", "002", String(airMoistureSensorValue));
}

bool sensorsNeedToBeChecked() {
  unsigned long currentTime = millis();

  if ((currentTime - lastCheck) >= checkInterval) {
    debug(String(currentTime) + " " + String(lastCheck));
    lastCheck = currentTime;
    return true;
  }
  return false;
}

void sendDataToAWS(String sensorType, String sensorId, String sensorValue) {
  DynamicJsonDocument sensorData(1024);
  String payload;

  sensorData["data_type"] = "sensor";
  sensorData["sensor_type"] = sensorType;
  sensorData["sensor_id"] = sensorId;
  sensorData["sensor_value"] = sensorValue;
  serializeJson(sensorData, payload);

  HTTPClient httpClient;
  httpClient.addHeader("Content-Type", "application/json");
  httpClient.begin(awsBaseUrl + greenhouseEndpoint);
  int response = httpClient.POST(payload);
  Serial.println("Response Code " + String(response));
  Serial.println(payload);
}

void initSerial() {
  Serial.begin(115200);
}

void connectToWiFi() {
  WiFiManager wm;
  WiFiManagerParameter awsBaseUrlTextbox("aws_endpoint", "AWS BaseUrl", "", 500);

  wm.addParameter(&awsBaseUrlTextbox);

  wm.resetSettings();

  bool res;
  res = wm.autoConnect("GreenhouseAP");

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    awsBaseUrl = awsBaseUrlTextbox.getValue();
    Serial.println("");
    Serial.print("Connected to WiFi: ");
    Serial.println(wm.getWiFiSSID());
    Serial.println("");
    Serial.print("WiFi IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("AWS Base url set to: ");
    Serial.print(awsBaseUrl);
  }

  delay(500);
}

void initInternalTime() {
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("Time initialized with ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  // %FT%T%z
  Serial.println(&timeinfo, "%FT%T%z");
}

void checkWiFiStatus() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection to Wifi lost.");
  }
}

void debug(String message) {
  if (isDebugEnabled) {
    Serial.println(message);
  }
}
