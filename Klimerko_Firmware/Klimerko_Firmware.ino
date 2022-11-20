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
String         firmwareVersion         = "2.1.0";
const char*    firmwareVersionPortal   =  "<p>Firmware Version: 2.1.0</p>";
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
char*          TEMP_OFFSET_ASSET       = "temperature-offset";
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
uint8_t        dataPublishInterval     = 15;    // [MINUTES] Default sensor data sending interval
const uint8_t  sensorAverageSamples    = 10;    // Number of samples used to average values from sensors
const int      sensorRetriesUntilConsideredOffline = 3;
bool           dataPublishFailed       = false; // Keeps track if a payload has failed to send so we can retry
unsigned long  sensorReadTime, dataPublishTime;

// -------------------------- PMS7003 -----------------------------------------------------
const uint8_t  pmsWakeBefore           = 30;    // [SECONDS] Seconds PMS sensor should be active before reading it
bool           pmsSensorOnline         = true;
int            pmsSensorRetry          = 0;
bool           pmsNoSleep              = false;
bool           pmsWoken                = false;
const char     *airQuality, *airQualityRaw;
int            avgPM1, avgPM25, avgPM10;

// -------------------------- BME280 -----------------------------------------------------
bool           bmeSensorOnline                = true;
int            bmeSensorRetry                 = 0;
float          bmeTemperatureOffset           = -4;   // Default temperature offset
char           bmeTemperatureOffsetChar[8];           // Used for WiFi Configuration Portal and memory
const char     bmeTemperatureOffsetDefault[8] = "-4"; // Used for WiFi Configuration Portal and memory
const int      bmeTemperatureOffsetMax        = 25;
const int      bmeTemperatureOffsetMin        = -25;
float          avgTemperature, avgHumidity, avgPressure;

// -------------------------- MEMORY -----------------------------------------------------
const uint16_t EEPROM_attStartAddress  = 0;
const uint16_t EEPROMsize              = 256;

// -------------------------- OBJECTS -----------------------------------------------------
WiFiManager wm;
WiFiManagerParameter portalDeviceID("device_id", "AllThingsTalk Device ID", deviceId, 32);
WiFiManagerParameter portalDeviceToken("device_token", "AllThingsTalk Device Token", deviceToken, 64);
WiFiManagerParameter portalTemperatureOffset("temperature_offset", "Temperature Offset", bmeTemperatureOffsetChar, 8);
WiFiManagerParameter portalDisplayFirmwareVersion(firmwareVersionPortal);
WiFiManagerParameter portalDisplayCredits("Firmware Designed and Developed by Vanja Stanic");
WiFiClient networkClient;
PubSubClient mqtt(networkClient);
SoftwareSerial pmsSerial(pmsTX, pmsRX);
PMS pms(pmsSerial);
PMS::DATA data;
Adafruit_BME280 bme;
movingAvg pm1(sensorAverageSamples);
movingAvg pm25(sensorAverageSamples);
movingAvg pm10(sensorAverageSamples);
movingAvg temp(sensorAverageSamples);
movingAvg hum(sensorAverageSamples);
movingAvg pres(sensorAverageSamples);

void sensorLoop() { // Reads and publishes sensor data and wakes up pms sensor in predefined intervals
  // Check if it's time to wake up PMS7003
  if (millis() - sensorReadTime >= readIntervalMillis() - (pmsWakeBefore * 1000) && !pmsWoken && pmsSensorOnline) {
    Serial.println("[PMS] Now waking up Air Quality Sensor");
    pmsPower(true);
  }
  
  // Read sensor data
  if (millis() - sensorReadTime >= readIntervalMillis()) {
    sensorReadTime = millis();
    readSensorData();
  }

  // Send average sensor data
  if (millis() - dataPublishTime >= dataPublishInterval * 60000) {
    if (!wifiConnectionLost) {
      if (!mqttConnectionLost) {
        dataPublishFailed = false;
        dataPublishTime = millis();
        publishSensorData();
      } else {
        if (!dataPublishFailed) {
          Serial.println("[DATA] Can't send sensor data because Klimerko is not connected to AllThingsTalk");
          dataPublishFailed = true;
        }
      }
    } else {
      if (!dataPublishFailed) {
        Serial.println("[DATA] Can't send sensor data because Klimerko is not connected to WiFi");
        dataPublishFailed = true;
      }
    }
  }
}

void readSensorData() {
  Serial.println("------------------------------DATA------------------------------");
  readPMS();
  readBME();
  Serial.println("----------------------------------------------------------------");
  if (!pmsNoSleep && pmsSensorOnline) {
    Serial.print("[PMS] Air Quality Sensor will sleep until ");
    Serial.print(pmsWakeBefore);
    Serial.println(" seconds before next reading.");
    pmsPower(false);
  }
}

void publishSensorData() {
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
    Serial.print(" µg/m³ (Average: ");
    Serial.print(avgPM1);
    Serial.println(")");
    Serial.print("PM 2.5:        ");
    Serial.print(PM2_5);
    Serial.print(" µg/m³ (Average: ");
    Serial.print(avgPM25);
    Serial.println(")");
    Serial.print("PM 10:         ");
    Serial.print(PM10);
    Serial.print(" µg/m³ (Average: ");
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
  float temperatureRaw = bme.readTemperature();
  float temperature    = temperatureRaw + bmeTemperatureOffset;
  float humidityRaw    = bme.readHumidity();
  float humidity       = humidityRaw * exp(243.12 * 17.62 * (temperatureRaw - temperature) / (243.12 + temperatureRaw) / (243.12 + temperature)); // Compensates the RH in accordance to temperature offset so the RH isn't wrong when the temp is offset
  float pressure       = bme.readPressure() / 100.0F;

  if (temperatureRaw > -100 && temperatureRaw < 150 && humidity >= 0 && humidity <= 100) {
    avgTemperature = temp.reading(temperature*100);
    avgTemperature = avgTemperature/100;
    avgHumidity    = hum.reading(humidity*100);
    avgHumidity    = avgHumidity/100;
    avgPressure    = pres.reading(pressure*100);
    avgPressure    = avgPressure/100;

    Serial.print("Temperature:   ");
    Serial.print(temperature);
    Serial.print("°C (Average: ");
    Serial.print(avgTemperature);
    Serial.print(", Raw: ");
    Serial.print(temperatureRaw);
    Serial.print(", Offset: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println(")");
    Serial.print("Humidity:      ");
    Serial.print(humidity);
    Serial.print(" % (Average: ");
    Serial.print(avgHumidity);
    Serial.print(", Raw: ");
    Serial.print(humidityRaw);
    Serial.println(")");
    Serial.print("Pressure:      ");
    Serial.print(pressure);
    Serial.print(" mbar (Average: ");
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
    dataPublishInterval = interval;
    pmsNoSleep = false;
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishDiagnosticData();
  } else if (interval <= 1) {
    dataPublishInterval = 1;
    pmsNoSleep = true;
    pmsPower(true);
    Serial.println("[DATA] Reporting interval set to 1 minute (minimum).");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("[DATA] This prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishDiagnosticData();
  } else if (interval <= 5) {
    dataPublishInterval = interval;
    pmsNoSleep = true;
    pmsPower(true);
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("[DATA] This prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishDiagnosticData();
  } else if (interval >= 60) {
    dataPublishInterval = 60;
    pmsNoSleep = false;
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(dataPublishInterval);
    Serial.println(" minutes");
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
      JsonObject dataPublishIntervalJson = doc.createNestedObject(INTERVAL_ASSET);
      dataPublishIntervalJson["value"] = dataPublishInterval;
      JsonObject firmwareJson = doc.createNestedObject(FIRMWARE_ASSET);
      firmwareJson["value"] = firmwareVersion;
      JsonObject wifiJson = doc.createNestedObject(WIFI_SIGNAL_ASSET);
      wifiJson["value"] = wifiSignal();
      JsonObject tempOffsetJson = doc.createNestedObject(TEMP_OFFSET_ASSET);
      tempOffsetJson["value"] = bmeTemperatureOffset;
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
  int result = (dataPublishInterval * 60000) / sensorAverageSamples;
  return result;
}

int readIntervalSeconds() {
  int result = (dataPublishInterval * 60) / sensorAverageSamples;
  return result;
}

void restoreData() { // Restores AllThingsTalk credentials from EEPROM as well as temperature offset data
  char okCreds[2+1];
  char okOffset[2+1];
  EEPROM.begin(EEPROMsize);
  EEPROM.get(EEPROM_attStartAddress, deviceId);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), okCreds);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(okCreds), bmeTemperatureOffsetChar);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(okCreds)+sizeof(bmeTemperatureOffsetChar), okOffset);
  EEPROM.end();
  if (String(okCreds) != String("OK")) {
    deviceId[0] = 0;
    deviceToken[0] = 0;
    Serial.println("[MEMORY] AllThingsTalk Device ID: Nothing in Memory");
  } else {
    Serial.print("[MEMORY] AllThingsTalk Device ID: ");
    Serial.println(deviceId);
//    portalDeviceID.setValue(deviceId, sizeof(deviceId)); // Set WiFi Configuration Portal to show real value
//    Serial.print("[MEMORY] AllThingsTalk Device Token: ");
//    Serial.println(deviceToken);
//    portalDeviceToken.setValue(deviceToken, sizeof(deviceToken)); // Set WiFi Configuration Portal to show real value
  }
  if (String(okOffset) != String("OK")) {
    Serial.print("[MEMORY] Temperature Offset: Nothing in Memory. Using default: ");
    Serial.println(bmeTemperatureOffset);
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar));
  } else {
    bmeTemperatureOffset = atof(bmeTemperatureOffsetChar); // Store the char that was in memory as a double (lazy)
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Update the value on WiFi Configuration Portal
    Serial.print("[MEMORY] Temperature Offset: ");
    Serial.print(bmeTemperatureOffsetChar);
    Serial.print("°C (Float: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println("°C)");
  }
}

void saveData() { // Saves new ATT credentials in memory and connects to AllThingsTalk
  Serial.println("[MEMORY] Saving data in persistent memory...");
  bool deviceIdCanBeSaved = false;
  bool deviceTokenCanBeSaved = false;
  bool tempOffsetCanBeSaved = false;

  if (sizeof(portalDeviceID.getValue()) >= sizeof(deviceId)) {
    Serial.print("[MEMORY] Won't save Device ID '");
    Serial.print(portalDeviceID.getValue());
    Serial.println("' because it's too long");
  } else if (String(portalDeviceID.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device ID because it's empty.");
  } else if (String(portalDeviceID.getValue()) == String(deviceId)) {
    Serial.print("[MEMORY] Won't save Device ID '");
    Serial.print(portalDeviceID.getValue());
    Serial.println("' because it's the same as the current one.");
  } else {
    sprintf(deviceId, "%s", portalDeviceID.getValue());
    Serial.print("[MEMORY] Saving Device ID: ");
    Serial.println(deviceId);
    deviceIdCanBeSaved = true;
  }

  if (sizeof(portalDeviceToken.getValue()) >= sizeof(deviceToken)) {
    Serial.print("[MEMORY] Won't save Device Token '");
    Serial.print(portalDeviceToken.getValue());
    Serial.println("' because it's too long");
  } else if (String(portalDeviceToken.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device Token because it's empty.");
  } else if (String(portalDeviceToken.getValue()) == String(deviceToken)) {
    Serial.print("[MEMORY] Won't save Device Token '");
    Serial.print(portalDeviceToken.getValue());
    Serial.println("' because it's the same as the current one.");
  } else {
    sprintf(deviceToken, "%s", portalDeviceToken.getValue());
    Serial.print("[MEMORY] Saving Device Token: ");
    Serial.println(deviceToken);
    deviceTokenCanBeSaved = true;
  }

  if (sizeof(portalTemperatureOffset.getValue()) >= sizeof(bmeTemperatureOffsetChar)) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.println("' because it's too long.");
  } else if (String(portalTemperatureOffset.getValue()) == String(bmeTemperatureOffsetChar)) {
    Serial.println("[MEMORY] Won't save Temperature Offset because it's the same as current value.");
  } else if (!isNumber(portalTemperatureOffset.getValue())) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.println("' because it's not a number.");
  } else if (atof(portalTemperatureOffset.getValue()) > bmeTemperatureOffsetMax) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.print("' because it's above the maximum of ");
    Serial.println(bmeTemperatureOffsetMax);
  } else if (atof(portalTemperatureOffset.getValue()) < bmeTemperatureOffsetMin) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.print("' because it's below the minimum of ");
    Serial.println(bmeTemperatureOffsetMin);
  } else if (String(portalTemperatureOffset.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Temperature Offset because it's empty.");
  } else {
    sprintf(bmeTemperatureOffsetChar, "%s", portalTemperatureOffset.getValue()); // Convert const char* to char array for saving in memory
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Set WiFi Configuration Portal to show the real value of offset
    bmeTemperatureOffset = atof(bmeTemperatureOffsetChar); // Convert the entered value to double (even though the variable is a float - I know, I know...)
    Serial.print("[MEMORY] Saving Temperature Offset: ");
    Serial.print(bmeTemperatureOffsetChar);
    Serial.print("°C (Float: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println("°C)");
    // Reset average temperature and humidity values in case the offset was changed during device operation since already-existing averaging data would be wrong due to new temperature offset.
    temp.reset();
    hum.reset();
    tempOffsetCanBeSaved = true;
  }

  portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Set WiFi Configuration Portal to show real value (in case user entered it wrong and it was disregarded)
  
  if (deviceIdCanBeSaved || deviceTokenCanBeSaved || tempOffsetCanBeSaved) {
    char ok[2+1] = "OK";
    EEPROM.begin(EEPROMsize);
    if (deviceIdCanBeSaved || deviceTokenCanBeSaved) {
      if (deviceIdCanBeSaved) {
        EEPROM.put(EEPROM_attStartAddress, deviceId);
      }
      if (deviceTokenCanBeSaved) {
        EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
      }
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), ok);
    }
    if (tempOffsetCanBeSaved) {
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(ok), bmeTemperatureOffsetChar);
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(ok)+sizeof(bmeTemperatureOffsetChar), ok);
    }
    if (EEPROM.commit()) {
      Serial.println("[MEMORY] Data saved.");
      EEPROM.end();
    } else {
      Serial.println("[MEMORY] Data couldn't be saved to memory.");
      EEPROM.end();
    }
  }
}

bool isNumber(const char* value) {
  if (value[0] != '-' && value[0] != '+' && !isDigit(value[0])) {
    return false;
  }
  if (value[1] != '\0') {
    int i = 1;
    do {
      if (!isDigit(value[i]) && value[i] != '.') {
        return false;
      }
      i++;
    } while(value[i] != '\0');
  }
  return true;
}

void connectAfterSavingData() {
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
  for (int i=EEPROM_attStartAddress; i <= sizeof(deviceId)+sizeof(deviceToken)+3+sizeof(bmeTemperatureOffsetChar)+3; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("[SYSTEM] Klimerko has been factory reset. All data has been erased. Rebooting in 5 seconds.");
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
    Serial.println("[WIFICONFIG] WiFi Configuration Mode Activated!");
  } else {
    Serial.println("[WIFICONFIG] WiFi Configuration Mode already active!");
  }
  Serial.print("[WIFICONFIG] Use your computer or smartphone to connect to WiFi network '");
  Serial.print(klimerkoID);
  Serial.print("' (password: '");
  Serial.print(wifiConfigPortalPassword);
  Serial.println("') to configure your Klimerko.");
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
  pmsPower(true);
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
  Serial.print("[WiFi] Connecting to WiFi... ");
  if(!wm.autoConnect(klimerkoID, wifiConfigPortalPassword)) {
    Serial.print("Failed! Reason: ");
    Serial.println(WiFi.status());
    wifiConnectionLost = true;
    return false;
  } else {
    Serial.print("Connected! IP: ");
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
    // AutoReconnect handles this, this here exists as backup
    if (millis() - wifiReconnectLastAttempt >= wifiReconnectInterval * 1000 && !wm.getConfigPortalActive()) {
      connectWiFi();
      wifiReconnectLastAttempt = millis();
    }
  }
}

void initWiFi() {
  wm.setDebugOutput(false);
  wm.addParameter(&portalDeviceID);
  wm.addParameter(&portalDeviceToken);
  wm.addParameter(&portalTemperatureOffset);
  wm.addParameter(&portalDisplayFirmwareVersion);
  wm.addParameter(&portalDisplayCredits);
  wm.setSaveParamsCallback(saveData);
  wm.setSaveConfigCallback(connectAfterSavingData);
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
  if (wm.getWiFiIsSaved()) {
    Serial.println((String)wm.getWiFiSSID());
  } else {
    Serial.println("Nothing in Memory");
  }
  connectWiFi();
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
  Serial.print(dataPublishInterval);
  Serial.println(" minutes. |");
  Serial.println(" --------------------------------------------------------------------------------");
  initAvg();
  initPins();
  initPMS();
  initBME();
  generateID();
  restoreData();
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
