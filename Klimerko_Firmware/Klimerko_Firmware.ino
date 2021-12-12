/*  ------------------------------------------- Project "KLIMERKO" ---------------------------------------------
 *  Citizen Air Quality measuring device with cloud monitoring, built at https://descon.me for the whole world.
 *  Programmed, built and maintained by Vanja Stanic // www.vanjastanic.com
 *  ------------------------------------------------------------------------------------------------------------
 *  This is a continued effort from https://descon.me/2018/winning-product/
 *  Supported by ISOC (Internet Society, Belgrade Chapter) // https://isoc.rs
 *  IoT Cloud Services and Communications SDK by AllThingsTalk // www.allthingstalk.com/
 *  3D Case for the device designed and manufactured by Dusan Nikic // nikic.dule@gmail.com
 *  ------------------------------------------------------------------------------------------------------------
 *  This sketch is downloaded from https://github.com/DesconBelgrade/Klimerko
 *  Head over there to read instructions and more about the project.
 *  Do not change anything in here unless you know what you're doing. Just upload this sketch to your device.
 *  You'll configure your WiFi and Cloud credentials once the sketch is uploaded to the device by 
 *  pressing the FLASH button on the NodeMCU for 2 seconds and connecting to Klimerko using any WiFi-enabled device.
 *  ------------------------------------------------------------------------------------------------------------
 *  Textual Air Quality Scale is based on PM10 criteria defined by RS Government (http://www.amskv.sepa.gov.rs/kriterijumi.php)
 *  Excellent (0-20), Good (21-40), Acceptable (41-50), Polluted (51-100), Very Polluted (Over 100)
 */

#include "src/AdafruitBME280/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include "src/movingAvg/movingAvg.h"
#include "src/WiFiManager/WiFiManager.h"
#include "src/PubSubClient/PubSubClient.h"
#include "src/ArduinoJson-v6.18.5.h"
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>

#define BUTTON_PIN     0
#define pmsTX          D5
#define pmsRX          D6

// ------------------------- Device -----------------------------------------------------
String         firmwareVersion         = "2.0.0";
const char*    firmwareVersionPortal   =  "<p>Firmware Version: 2.0.0</p>";
char           klimerkoID[32];

// -------------------------- WiFi ------------------------------------------------------
const int      wifiReconnectInterval   = 60;
bool           wifiConnectionLost      = true;
unsigned long  wifiReconnectLastAttempt;

// ------------------- WiFi Configuration Portal ----------------------------------------
char const     *wifiConfigPortalPassword = "ConfigMode"; // Password for WiFi Configuration Portal WiFi Network
const int      wifiConfigTimeout       = 1800;         // Seconds before WiFi Configuration expires
unsigned long  wifiConfigActiveSince;

// -------------------------- MQTT ------------------------------------------------------
const char*    MQTT_SERVER             = "api.allthingstalk.io";
const uint16_t MQTT_PORT               = 1883;
const char*    MQTT_PASSWORD           = "arbitrary";
uint16_t       MQTT_MAX_MESSAGE_SIZE   = 2048;
char           deviceId[32], deviceToken[64];

const int      mqttReconnectInterval   = 30; // Seconds between retries
bool           mqttConnectionLost      = true;
unsigned long  mqttReconnectLastAttempt;

char*          PM1_ASSET               = "pm1";
char*          PM2_5_ASSET             = "pm2-5";
char*          PM10_ASSET              = "pm10";
char*          AQ_ASSET                = "air-quality";
char*          TEMPERATURE_ASSET       = "temperature";
char*          HUMIDITY_ASSET          = "humidity";
char*          PRESSURE_ASSET          = "pressure";
char*          INTERVAL_ASSET          = "interval";
char*          FIRMWARE_ASSET          = "firmware";
char*          WIFI_SIGNAL_ASSET       = "wifi-signal";

// -------------------------- BUTTON ------------------------------------------------------
const int      buttonLongPressTime     = 15000; // (milliseconds) Everything above this is considered a long press
const int      buttonMediumPressTime   = 1000;  // (milliseconds) Everything above this and below long press time is considered a medium press
const int      buttonShortPressTime    = 50;    // (milliseconds) Everything above this and below medium press time is considered a short press
unsigned long  buttonPressedTime       = 0;
unsigned long  buttonReleasedTime      = 0;
bool           buttonPressed           = false;
bool           buttonLongPressDetected = false;
int            buttonLastState         = HIGH;
int            buttonCurrentState;

// -------------------------- LED ---------------------------------------------------------
bool           ledState                = false;
bool           ledSuccessBlink         = false;
const int      ledBlinkInterval        = 1000;  // (milliseconds) How often to blink LED when there's no connection
unsigned long  ledLastUpdate;

// --------------------- SENSORS (GENERAL) ------------------------------------------------
uint8_t        sendInterval            = 15;    // [MINUTES] Default sensor data sending interval
const uint8_t  averageSamples          = 10;    // Number of samples used to average values from sensors
const int      sensorRetriesUntilConsideredOffline = 2;

// -------------------------- PMS7003 -----------------------------------------------------
const uint8_t  wakeInterval            = 30;    // [SECONDS] Seconds PMS sensor should be active before reading it
bool           pmsSensorOnline         = true;
int            pmsSensorRetry          = 0;
bool           pmsNoSleep              = false;
bool           pmsWoken                = false;
const char     *airQuality, *airQualityRaw;
int            avgPM1, avgPM25, avgPM10;
unsigned long  lastReadTime, lastSendTime;

// -------------------------- BME280 -----------------------------------------------------
bool           bmeSensorOnline         = true;
int            bmeSensorRetry          = 0;
int            avgTemperature, avgHumidity, avgPressure;

// -------------------------- MEMORY -----------------------------------------------------
const uint16_t EEPROM_attStartAddress  = 0;
const uint16_t EEPROMsize              = 256;

// -------------------------- OBJECTS -----------------------------------------------------
WiFiManager wm;
WiFiManagerParameter portalDeviceID("device_id", "AllThingsTalk Device ID", deviceId, 32);
WiFiManagerParameter portalDeviceToken("device_token", "AllThingsTalk Device Token", deviceToken, 64);
WiFiManagerParameter portalDisplayFirmwareVersion(firmwareVersionPortal);
WiFiManagerParameter portalDisplayCredits("Firmware Designed and Developed by Vanja Stanic");
WiFiClient networkClient;
PubSubClient mqtt(networkClient);
SoftwareSerial pmsSerial(pmsTX, pmsRX);
PMS pms(pmsSerial);
PMS::DATA data;
Adafruit_BME280 bme;
movingAvg pm1(averageSamples);
movingAvg pm25(averageSamples);
movingAvg pm10(averageSamples);
movingAvg temp(averageSamples);
movingAvg hum(averageSamples);
movingAvg pres(averageSamples);

void sensorLoop() { // Reads and publishes sensor data and wakes up pms sensor in predefined intervals
  // Check if it's time to wake up PMS7003
  if (millis() - lastReadTime >= readIntervalMillis() - (wakeInterval * 1000) && !pmsWoken && pmsSensorOnline) {
    Serial.println("[PMS] Now waking up Air Quality Sensor");
    pmsPower(true);
  }
  
  // Read sensor data
  if (millis() - lastReadTime >= readIntervalMillis()) {
    readSensorData();
  }

  // Send average sensor data
  if (millis() - lastSendTime >= sendInterval * 60000) {
    publishSensorData();
    lastSendTime = millis();
  }
}

void readSensorData() {
  Serial.println("------------------------------DATA------------------------------");
  readPMS();
  readBME();
  Serial.println("----------------------------------------------------------------");
  if (!pmsNoSleep && pmsSensorOnline) {
    Serial.print("[PMS] Air Quality Sensor will sleep until ");
    Serial.print(wakeInterval);
    Serial.println(" seconds before next reading.");
    pmsPower(false);
  }
  lastReadTime = millis();
}

void publishSensorData() {
  if (!wifiConnectionLost) {
    if (!mqttConnectionLost) {
      Serial.println("[DATA] Now sending averaged data to AllThingsTalk...");
      char JSONmessageBuffer[512];
      DynamicJsonDocument doc(512);
      if (pmsSensorOnline) {
        JsonObject airQualityJson = doc.createNestedObject(AQ_ASSET);
        airQualityJson["value"] = airQuality;
        JsonObject pm1Json = doc.createNestedObject(PM1_ASSET);
        pm1Json["value"] = avgPM1;
        JsonObject pm25Json = doc.createNestedObject(PM2_5_ASSET);
        pm25Json["value"] = avgPM25;
        JsonObject pm10Json = doc.createNestedObject(PM10_ASSET);
        pm10Json["value"] = avgPM10;
      } else {
        Serial.println("[DATA] Won't send Air Quality Sensor (PMS7003) data because it seems to be offline.");
      }
      if (bmeSensorOnline) {
        JsonObject temperatureJson = doc.createNestedObject(TEMPERATURE_ASSET);
        temperatureJson["value"] = avgTemperature;
        JsonObject humidityJson = doc.createNestedObject(HUMIDITY_ASSET);
        humidityJson["value"] = avgHumidity;
        JsonObject pressureJson = doc.createNestedObject(PRESSURE_ASSET);
        pressureJson["value"] = avgPressure;
      } else {
        Serial.println("[DATA] Won't send Temperature/Humidity/Pressure Sensor (BME280) data because it seems to be offline.");
      }
      JsonObject firmwareJson = doc.createNestedObject(FIRMWARE_ASSET);
      firmwareJson["value"] = firmwareVersion;
      JsonObject wifiJson = doc.createNestedObject(WIFI_SIGNAL_ASSET);
      wifiJson["value"] = wifiSignal();
      serializeJson(doc, JSONmessageBuffer);
  
      char topic[128];
      snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceId, "/state");
      mqtt.publish(topic, JSONmessageBuffer, false);
      Serial.print("[DATA] Published sensor data to AllThingsTalk: ");
      Serial.println(JSONmessageBuffer);
      
      Serial.println("");
    } else {
      Serial.println("[DATA] Can't send data because Klimerko is not connected to AllThingsTalk");
    }
  } else {
    Serial.println("[DATA] Can't send data because Klimerko is not connected to WiFi");
  }
}

void readPMS() { // Function that reads data from the PMS7003
  while (pmsSerial.available()) { pmsSerial.read(); }
  pms.requestRead(); // Now get the real data
  
  if (pms.readUntil(data)) {
    int PM1 = data.PM_AE_UG_1_0;
    int PM2_5 = data.PM_AE_UG_2_5;
    int PM10 = data.PM_AE_UG_10_0;

    avgPM1 = pm1.reading(PM1);
    avgPM25 = pm25.reading(PM2_5);
    avgPM10 = pm10.reading(PM10);

    // Assign a text value of how good the air is based on current value
    // http://www.amskv.sepa.gov.rs/kriterijumi.php
    if (PM10 <= 20) {
      airQualityRaw = "Excellent";
    } else if (PM10 >= 21 && PM10 <= 40) {
      airQualityRaw = "Good";
    } else if (PM10 >= 41 && PM10 <= 50) {
      airQualityRaw = "Acceptable";
    } else if (PM10 >= 51 && PM10 <= 100) {
      airQualityRaw = "Polluted";
    } else if (PM10 > 100) {
      airQualityRaw = "Very Polluted";
    }

    // Assign a text value of how good the air is based on average value
    // http://www.amskv.sepa.gov.rs/kriterijumi.php
    if (avgPM10 <= 20) {
      airQuality = "Excellent";
    } else if (avgPM10 >= 21 && avgPM10 <= 40) {
      airQuality = "Good";
    } else if (avgPM10 >= 41 && avgPM10 <= 50) {
      airQuality = "Acceptable";
    } else if (avgPM10 >= 51 && avgPM10 <= 100) {
      airQuality = "Polluted";
    } else if (avgPM10 > 100) {
      airQuality = "Very Polluted";
    }

    // Print via SERIAL
    Serial.print("Air Quality is ");
    Serial.print(airQualityRaw);
    Serial.print(" (Average: ");
    Serial.print(airQuality);
    Serial.println(")");
    Serial.print("PM 1:          ");
    Serial.print(PM1);
    Serial.print(" (Average: ");
    Serial.print(avgPM1);
    Serial.println(")");
    Serial.print("PM 2.5:        ");
    Serial.print(PM2_5);
    Serial.print(" (Average: ");
    Serial.print(avgPM25);
    Serial.println(")");
    Serial.print("PM 10:         ");
    Serial.print(PM10);
    Serial.print(" (Average: ");
    Serial.print(avgPM10);
    Serial.println(")");

    pmsSensorRetry = 0;
    if (!pmsSensorOnline) {
      pmsSensorOnline = true;
      Serial.println("[PMS] Air Quality Sensor (PMS7003) seems to be back online!");
    }
  } else {
    if (pmsSensorOnline) {
      Serial.println("[PMS] Air Quality Sensor (PMS7003) returned no data on data request this time.");
      pmsSensorRetry++;
      if (pmsSensorRetry > sensorRetriesUntilConsideredOffline) {
        pmsSensorOnline = false;
        Serial.println("[PMS] Air Quality Sensor (PMS7003) seems to be offline!");
        pm1.reset();
        pm25.reset();
        pm10.reset();
        initPMS();
      }
    } else {
      Serial.println("[PMS] Air Quality Sensor (PMS7003) is offline.");
      initPMS();
    }
  }
}

void readBME() { // Function for reading data from the BME280 Sensor
  // Save the current BME280 sensor data
  // BME280 Heats itself up about 1-2 degrees and the NodeMCU
  // inside the Klimerko's 3D case interferes by about 2 degrees
  // Because of this, we'll subtract 4 degrees from the raw reading
  // (do note that the calibration/testing was done at 20*-25* celsius)
  float temperatureRaw =  bme.readTemperature() - 4;
  float humidityRaw = bme.readHumidity();
  float pressureRaw = bme.readPressure() / 100.0F;

  if (temperatureRaw > -100 && temperatureRaw < 150 && humidityRaw >= 0 && humidityRaw <= 100) {
    static char temperature[7];
    dtostrf(temperatureRaw, 6, 1, temperature);
    static char humidity[7];
    dtostrf(humidityRaw, 6, 1, humidity);
    static char pressure[7];
    dtostrf(pressureRaw, 6, 0, pressure);

    // Add to average calculation and load current average
    avgTemperature = temp.reading(temperatureRaw);
    avgHumidity = hum.reading(humidityRaw);
    avgPressure = pres.reading(pressureRaw);

    // Print data to Serial port
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" (Average: ");
    Serial.print(avgTemperature);
    Serial.println(")");
    Serial.print("Humidity:    ");
    Serial.print(humidity);
    Serial.print(" (Average: ");
    Serial.print(avgHumidity);
    Serial.println(")");
    Serial.print("Pressure:    ");
    Serial.print(pressure);
    Serial.print(" (Average: ");
    Serial.print(avgPressure);
    Serial.println(")");

    bmeSensorRetry = 0;
    if (!bmeSensorOnline) {
      bmeSensorOnline = true;
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) is back online!");
    }
  } else {
    if (bmeSensorOnline) {
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) returned no data this time.");
      bmeSensorRetry++;
      if (bmeSensorRetry > sensorRetriesUntilConsideredOffline) {
        bmeSensorOnline = false;
        Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) seems to be offline!");
        temp.reset();
        hum.reset();
        pres.reset();
        initBME();
      }
    } else {
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) is offline.");
      initBME();
    }
  }
}

void pmsPower(bool state) { // Controls sleep state of PMS sensor
  if (state) {
    pms.wakeUp();
    pms.passiveMode();
    pmsWoken = true;
  } else {
    pmsSerial.flush();
    unsigned long now = millis();
    while(millis() < now + 100);
    pmsWoken = false;
    pms.sleep();
  }
}
 
void changeInterval(int interval) { // Changes sensor data reporting interval
  if (interval > 5 && interval <= 60) {
    sendInterval = interval;
    pmsNoSleep = false;
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishDiagnosticData();
  } else if (interval <= 5) {
    sendInterval = 5;
    pmsNoSleep = true;
    pmsPower(true);
    Serial.println("[DATA] Reporting interval set to 5 minutes (minimum).");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("[DATA] Note that this prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishDiagnosticData();
  } else if (interval > 60) {
    pmsNoSleep = false;
    Serial.print("[DATA] Received command to set reporting interval to ");
    Serial.print(interval);
    Serial.println(" minutes but that exceeds the maximum.");
    Serial.print("[DATA] Reverted back to ");
    Serial.print(sendInterval);
    Serial.println(" minutes.");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishDiagnosticData();
  }
}

void publishDiagnosticData() { // Publishes diagnostic data to AllThingsTalk
  if (!wifiConnectionLost) {
    if (!mqttConnectionLost) {
      char JSONmessageBuffer[256];
      DynamicJsonDocument doc(256);
      JsonObject sendIntervalJson = doc.createNestedObject(INTERVAL_ASSET);
      sendIntervalJson["value"] = sendInterval;
      JsonObject firmwareJson = doc.createNestedObject(FIRMWARE_ASSET);
      firmwareJson["value"] = firmwareVersion;
      JsonObject wifiJson = doc.createNestedObject(WIFI_SIGNAL_ASSET);
      wifiJson["value"] = wifiSignal();
      serializeJson(doc, JSONmessageBuffer);
    
      char topic[256];
      snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceId, "/state");
      mqtt.publish(topic, JSONmessageBuffer, false);
      Serial.print("[DATA] Published diagnostic data to AllThingsTalk: ");
      Serial.println(JSONmessageBuffer);
    } else {
      Serial.println("[DATA] Can't send diagnostic data because Klimerko is not connected to AllThingsTalk");
    }
  } else {
    Serial.println("[DATA] Can't send diagnostic data because Klimerko is not connected to WiFi");
  }
}

int readIntervalMillis() {
  int result = (sendInterval * 60000) / averageSamples;
  return result;
}

int readIntervalSeconds() {
  int result = (sendInterval * 60) / averageSamples;
  return result;
}

void getCredentials() { // Restores AllThingsTalk credentials from EEPROM
  EEPROM.begin(EEPROMsize);
  EEPROM.get(EEPROM_attStartAddress, deviceId);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
  char ok[2+1];
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    deviceId[0] = 0;
    deviceToken[0] = 0;
  }
  Serial.print("[MEMORY] AllThingsTalk Device ID: ");
  Serial.println(deviceId);
//  Serial.print("[MEMORY] AllThingsTalk Device Token: ");
//  Serial.println(deviceToken);
}

void saveCredentials() { // Saves new ATT credentials in memory and connects to AllThingsTalk
  Serial.println("[MEMORY] Now saving AllThingsTalk Credentials in memory.");
  bool deviceIdCanBeSaved = false;
  bool deviceTokenCanBeSaved = false;
  
  if (sizeof(portalDeviceID.getValue()) >= sizeof(deviceId)) {
    Serial.print("[MEMORY] Won't save Device ID '");
    Serial.print(deviceToken);
    Serial.println("' because it's too long");
  } else if (String(portalDeviceID.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device ID because it's empty.");
  } else {
    sprintf(deviceId, "%s", portalDeviceID.getValue());
    deviceIdCanBeSaved = true;
    Serial.print("[MEMORY] Saving Device ID: ");
    Serial.println(deviceId);
  }

  if (sizeof(portalDeviceToken.getValue()) >= sizeof(deviceToken)) {
    Serial.print("[MEMORY] Won't save Device Token '");
    Serial.print(deviceToken);
    Serial.println("' because it's too long");
  } else if (String(portalDeviceToken.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device Token because it's empty.");
  } else {
    sprintf(deviceToken, "%s", portalDeviceToken.getValue());
    deviceTokenCanBeSaved = true;
    Serial.print("[MEMORY] Saving Device Token: ");
    Serial.println(deviceToken);
  }
  
  if (deviceIdCanBeSaved || deviceTokenCanBeSaved) {
    EEPROM.begin(EEPROMsize);
    if (deviceIdCanBeSaved) { 
      EEPROM.put(EEPROM_attStartAddress, deviceId);
    }
    if (deviceTokenCanBeSaved) {
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
    }
    char ok[2+1] = "OK";
    EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), ok);
    if (EEPROM.commit()) {
      Serial.println("[MEMORY] Data saved.");
      EEPROM.end();
    } else {
      Serial.println("[MEMORY] Data couldn't be saved to memory.");
      EEPROM.end();
    }
  }
}

void connectAfterNewCredentials() {
  connectMQTT();
}

void factoryReset() { // Deletes WiFi and AllThingsTalk credentials and reboots Klimerko
  for (int i=0;i<40;i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  wm.resetSettings();
  ESP.eraseConfig();
  EEPROM.begin(EEPROMsize);
  for (int i=0; i <= sizeof(deviceId)+sizeof(deviceToken)+3; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("KLIMERKO HAS BEEN FACTORY RESET. ALL CREDENTIALS HAVE BEEN DELETED. REBOOTING IN 5 SECONDS...");
  delay(5000);
  ESP.restart();
}

void wifiConfigWebServerStarted() {
  wm.server->on("/exit", wifiConfigStop); // If user presses Exit, turn off WiFi Configuration Portal.
}

void wifiConfigStarted(WiFiManager *wm) { // Called once WiFi Configuration Portal is started
  wifiConfigActiveSince = millis();
}

void wifiConfigStart() { // Starts WiFi Configuration Portal
  if (!wm.getConfigPortalActive()) {
    Serial.println("[WIFICONFIG] Entering WiFi Configuration Mode...");
    wm.startConfigPortal(klimerkoID, wifiConfigPortalPassword);
    Serial.print("[WIFICONFIG] WiFi Configuration Mode Active! Use your phone/computer to connect to WiFi network named '");
    Serial.print(klimerkoID);
    Serial.println("' and to configure your Klimerko.");
  } else {
    Serial.print("[WIFICONFIG] WiFi Configuration Mode is already active! Use your phone/computer to connect to WiFi network named '");
    Serial.print(klimerkoID);
    Serial.println("' and to configure your Klimerko.");
  }
}

void wifiConfigStop() { // Stops WiFi Configuration Portal
  if (wm.getConfigPortalActive()) {
    wm.stopConfigPortal();
    Serial.println("[WIFICONFIG] WiFi Configuration Portal has been stopped.");
  } else {
    Serial.println("[WIFICONFIG] Can't stop WiFi Configuration Portal because it's not running.");
  }
}

void wifiConfigLoop() { // Keep WiFi Configurartion mode portal in the loop if it's supposed to be active
  if (wm.getConfigPortalActive()) {
    wm.process();
     if (millis() - wifiConfigActiveSince >= wifiConfigTimeout * 1000) {
       Serial.println("[WIFICONFIG] WiFi Configuration Mode Expired.");
       wifiConfigStop();
     }
  }
}

void buttonLoop() { // Handles the FLASH button and all it's features
  buttonCurrentState = digitalRead(BUTTON_PIN);
  if (buttonLastState == HIGH && buttonCurrentState == LOW) {
    buttonPressedTime = millis();
    buttonPressed = true;
    buttonLongPressDetected = false;
    // Button is being pressed at the moment
  } else if (buttonLastState == LOW && buttonCurrentState == HIGH) {
    buttonReleasedTime = millis();
    buttonPressed = false;
    long buttonPressDuration = buttonReleasedTime - buttonPressedTime;
    if (buttonPressDuration > buttonShortPressTime && buttonPressDuration < buttonMediumPressTime && buttonPressDuration < buttonLongPressTime) {
      Serial.println("[BUTTON] Short Press Detected!");
      wifiConfigStop();
    } else if (buttonPressDuration > buttonShortPressTime && buttonPressDuration > buttonMediumPressTime && buttonPressDuration < buttonLongPressTime) {
      Serial.println("[BUTTON] Long Press Detected!");
      wifiConfigStart();
    }
  }

  if (buttonPressed && !buttonLongPressDetected) {
    if (millis() - buttonPressedTime > buttonLongPressTime) {
      buttonLongPressDetected = true;
      Serial.println("[BUTTON] Super Long Press Detected!");
      factoryReset();
    }
  }
  buttonLastState = buttonCurrentState;
}

void ledLoop() { // Handles status LED
  if (ledSuccessBlink) {
    for (int i=0;i<6;i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
    ledSuccessBlink = false;
  }
  
  if (wm.getConfigPortalActive()) {
    ledState = true;
  } else {
    if (wifiConnectionLost || mqttConnectionLost) {
      if (millis() - ledLastUpdate >= ledBlinkInterval) {
        ledState = !ledState;
        ledLastUpdate = millis();
      }
    } else {
      ledState = false;
    }
  }
  
  if (ledState) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

String extractAssetNameFromTopic(String topic) {
  const int devicePrefixLength = 38;
  const int stateSuffixLength = 8;
  return topic.substring(devicePrefixLength, topic.length()-stateSuffixLength);
}

void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length) {
  Serial.println("[MQTT] Message Received from AllThingsTalk");
  String topic(p_topic);
  
  // Deserialize JSON
  DynamicJsonDocument doc(256);
  char json[256];
  for (int i = 0; i < p_length; i++) {
      json[i] = (char)p_payload[i];
  }
  auto error = deserializeJson(doc, json);
  if (error) {
      Serial.print("[MQTT] Parsing JSON failed. Code: ");
      Serial.println(error.c_str());
      return;
  }

  String asset = extractAssetNameFromTopic(topic);
//  Serial.print("[MQTT] Asset Name: ");
//  Serial.println(asset);

  if (asset == INTERVAL_ASSET) {
    int value = doc["value"];
    changeInterval(value);
  }
}

String wifiSignal() {
  if (!wifiConnectionLost) {
    int signal = WiFi.RSSI();
    String signalString;
    if (signal < -87) {
        signalString = "Horrible";
    } else if (signal >= -87 && signal <= -80) {
        signalString = "Bad";
    } else if (signal > -80 && signal <= -70) {
        signalString = "Decent";
    } else if (signal > -70 && signal <= -55) {
        signalString = "Good";
    } else if (signal > -55) {
        signalString = "Excellent";
    } else {
        signalString = "Error";
    }
    return signalString;
  }
}

void initPMS() {
  pmsSerial.begin(9600);
  pmsPower(false);
}

void initBME() {
  bme.begin(0x76);
}

void generateID() {
  snprintf(klimerkoID, sizeof(klimerkoID), "%s%i", "KLIMERKO-", ESP.getChipId());
  Serial.print("[ID] Unique Klimerko ID: ");
  Serial.println(klimerkoID);
}

void mqttSubscribeTopics() {
  char command_topic[256];
  snprintf(command_topic, sizeof command_topic, "%s%s%s", "device/", deviceId, "/asset/+/command");
  mqtt.subscribe(command_topic);
}

bool connectMQTT() {
  if (!wifiConnectionLost) {
    Serial.print("[MQTT] Connecting to AllThingsTalk... ");
    if (mqtt.connect(klimerkoID, deviceToken, MQTT_PASSWORD)) {
      Serial.println("Connected!");
      if (mqttConnectionLost) {
        mqttConnectionLost = false;
        ledSuccessBlink = true;
      }
      mqttSubscribeTopics();
      publishDiagnosticData();
      return true;
    } else {
      Serial.print("Failed! Reason: ");
      Serial.println(mqtt.state());
      mqttConnectionLost = true;
      return false;
    }
  }
  return false;
}

void maintainMQTT() {
  mqtt.loop();
  if (mqtt.connected()) {
    if (mqttConnectionLost) {
      mqttConnectionLost = false;
      publishDiagnosticData();
    }
  } else {
    if (!mqttConnectionLost) {
      if (wifiConnectionLost) {
        Serial.println("[MQTT] Lost connection due to WiFi!");
      } else {
        Serial.print("[MQTT] Lost Connection. Reason: ");
        Serial.println(mqtt.state());
      }
      mqttConnectionLost = true;
    }
    if (millis() - mqttReconnectLastAttempt >= mqttReconnectInterval * 1000 && !wifiConnectionLost) {
      connectMQTT();
      mqttReconnectLastAttempt = millis();
    }
  }
}

bool initMQTT() {
  mqtt.setBufferSize(MQTT_MAX_MESSAGE_SIZE);
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(mqttCallback);
  return connectMQTT();
}

bool connectWiFi() {
  Serial.println("[WiFi] Trying to connect...");
  if(!wm.autoConnect(klimerkoID, wifiConfigPortalPassword)) {
    Serial.print("[WiFi] Failed to connect! Reason: ");
    Serial.println(WiFi.status());
    wifiConnectionLost = true;
    return false;
  } else {
    Serial.print("[WiFi] Successfully Connected! IP: ");
    Serial.println(WiFi.localIP());
    wifiConnectionLost = false;
    ledSuccessBlink = true;
    return true;
  }
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiConnectionLost) {
      Serial.print("[WiFi] Connection Re-Established! IP: ");
      Serial.println(WiFi.localIP());
      wifiConnectionLost = false;
      ledSuccessBlink = true;
    }
  } else {
    if (!wifiConnectionLost) {
      Serial.print("[WiFi] Connection Lost! Reason: ");
      Serial.println(WiFi.status());
      wifiConnectionLost = true;
    }
//    // AutoReconnect handles this, but I left it here just in case it's needed again
//    if (millis() - wifiReconnectLastAttempt >= wifiReconnectInterval * 1000 && !wm.getConfigPortalActive()) {
//      connectWiFi();
//      wifiReconnectLastAttempt = millis();
//    }
  }
}

void initWiFi() {
  wm.setDebugOutput(false);
  wm.addParameter(&portalDeviceID);
  wm.addParameter(&portalDeviceToken);
  wm.addParameter(&portalDisplayFirmwareVersion);
  wm.addParameter(&portalDisplayCredits);
  wm.setSaveParamsCallback(saveCredentials);
  wm.setSaveConfigCallback(connectAfterNewCredentials);
  wm.setWebServerCallback(wifiConfigWebServerStarted);
  wm.setAPCallback(wifiConfigStarted);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(wifiConfigTimeout);
  wm.setConnectRetries(2);
  wm.setConnectTimeout(5);
  wm.setDarkMode(true);
  wm.setTitle("Klimerko");
  wm.setHostname(klimerkoID);
  wm.setCountry("RS");
  wm.setEnableConfigPortal(false);
  wm.setParamsPage(false);
  wm.setSaveConnect(true);
  wm.setBreakAfterConfig(true);
  wm.setWiFiAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.print("[MEMORY] WiFi SSID: ");
  Serial.println((String)wm.getWiFiSSID());
  connectWiFi();
//  // Start WiFi Configuration Mode automatically if there's no credentials stored
//  if (wm.getWiFiIsSaved()) {
//    Serial.println((String)wm.getWiFiSSID());
//    connectWiFi();
//  } else {
//    Serial.println("None");
//    wifiConnectionLost = true;
//    wifiConfigStart();
//  }
}

void initAvg() {
  pm1.begin();
  pm25.begin();
  pm10.begin();
  temp.begin();
  hum.begin();
  pres.begin();
}

void initPins() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println(" ------------------------------ Project 'KLIMERKO' ------------------------------");
  Serial.println("|                  https://github.com/DesconBelgrade/Klimerko                    |");
  Serial.println("|                               www.klimerko.org                                 |");
  Serial.print("|                           Firmware Version: ");
  Serial.print(firmwareVersion);
  Serial.println("                              |");
  Serial.println("|    Hold NodeMCU FLASH button for 2 seconds to enter WiFi Configuration Mode.   |");
  Serial.print("| Sensors are read every ");
  Serial.print(readIntervalSeconds());
  Serial.print(" seconds and averages are published every ");
  Serial.print(sendInterval);
  Serial.println(" minutes. |");
  Serial.println(" --------------------------------------------------------------------------------");
  initAvg();
  initPins();
  initPMS();
  initBME();
  generateID();
  getCredentials();
  initWiFi();
  initMQTT();
  Serial.println("");
}

void loop() {
  sensorLoop();
  maintainWiFi();
  maintainMQTT();
  wifiConfigLoop();
  buttonLoop();
  ledLoop();
}
