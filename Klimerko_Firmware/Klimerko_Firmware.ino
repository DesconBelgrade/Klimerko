/*  
 *   ------------------------------------------- Project "KLIMERKO" ---------------------------------------------
 *  Citizen Air Quality measuring device with cloud monitoring, built at https://descon.me for the whole world.
 *  Programmed, built and maintained by Vanja Stanic // www.vanjastanic.com
 *  
 *  This is a continued effort from https://descon.me/2018/winning-product/
 *  Supported by ISOC (Internet Society, Belgrade Chapter) // https://isoc.rs
 *  IoT Cloud Services and Communications SDK by AllThingsTalk // www.allthingstalk.com/
 *  3D Case for the device designed and manufactured by Dusan Nikic // nikic.dule@gmail.com
 *  ------------------------------------------------------------------------------------------------------------
 *  
 *  This sketch is downloaded from https://github.com/DesconBelgrade/Klimerko
 *  Head over there to read instructions and more about the project.
 *  Do not change anything in here unless you know what you're doing. Just upload this sketch to your device.
 *  You'll configure your WiFi and Cloud credentials once the sketch is uploaded to the device by 
 *  opening Serial Monitor, restarting the device (RST button on NodeMCU), writing "config" in Serial Monitor 
 *  and following the instructions shown in Serial Monitor.
 *  
 *  Textual Air Quality Scale is based on PM10 criteria defined by RS Government (http://www.amskv.sepa.gov.rs/kriterijumi.php)
 *  Excellent (0-20), Good (21-40), Acceptable (41-50), Polluted (51-100), Very Polluted (Over 100)
 */

#include "src/AllThingsTalk/AllThingsTalk_WiFi.h"
#include "src/AdafruitBME280/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include "src/movingAvg/movingAvg.h"
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>

String firmwareVersion = "1.3.0";
int sendInterval = 15; // [MINUTES] Default sensor data sending interval
int wakeInterval = 30; // [SECONDS] Seconds to activate sensor before reading it
int averageSamples = 10; // Number of samples used to average values from sensors
int deviceRestartWait = 3; // Seconds to wait when restarting NodeMCU
bool noSleep = false;
// Pins to which the PMS7003 is connected to
const uint8_t pmsTX = D5;
const uint8_t pmsRX = D6;
const char *airQuality, *airQualityRaw;
unsigned long currentTime, lastReadTime, lastSendTime;
int avgTemperature, avgHumidity, avgPressure, avgPM1, avgPM25, avgPM10;
bool firstReading = true;
bool pmsWoken = false;
String wifiName, wifiPassword, deviceId, deviceToken;
String wifiNameTemp, wifiPasswordTemp, deviceIdTemp, deviceTokenTemp;
int wifiName_EEPROM_begin = 0;
int wifiName_EEPROM_end = 255;
int wifiPassword_EEPROM_begin = 256;
int wifiPassword_EEPROM_end = 511;
int deviceId_EEPROM_begin = 512;
int deviceId_EEPROM_end = 767;
int deviceToken_EEPROM_begin = 768;
int deviceToken_EEPROM_end = 1024;
int EEPROMsize = 1024;
String dataDivider = ";";
// Assets to be published to
char* PM1_ASSET         = "pm1";
char* PM2_5_ASSET       = "pm2-5";
char* PM10_ASSET        = "pm10";
char* AQ_ASSET          = "air-quality";
char* TEMPERATURE_ASSET = "temperature";
char* HUMIDITY_ASSET    = "humidity";
char* PRESSURE_ASSET    = "pressure";
char* INTERVAL_ASSET    = "interval";

WifiCredentials wifiCreds = WifiCredentials("", ""); // Don't write anything here
DeviceConfig deviceCreds = DeviceConfig("", ""); // Don't write anything here
Device device = Device(wifiCreds, deviceCreds);
Adafruit_BME280 bme;
SoftwareSerial pmsSerial(pmsTX, pmsRX);
PMS pms(pmsSerial);
PMS::DATA data;
CborPayload payload;
movingAvg pm1(averageSamples);
movingAvg pm25(averageSamples);
movingAvg pm10(averageSamples);
movingAvg temp(averageSamples);
movingAvg hum(averageSamples);
movingAvg pres(averageSamples);

// Runs only at boot
void setup() {
  Serial.begin(115200);
  // The following gives the user time to open serial monitor
  // and his computer to recognize the COM port for klimerko
  // before outputting data
  for (int i=5; i > 0; i--) {
    Serial.print("Booting in ");
    Serial.print(i);
    Serial.println(" seconds");
    delay(1000);
  }
  ESP.eraseConfig();
  pmsSerial.begin(9600);
  pmsPower(false);
  bme.begin(0x76);
  Serial.println("");
  Serial.println(" ------------------------- Project 'KLIMERKO' ----------------------------");
  Serial.println("|              https://github.com/DesconBelgrade/Klimerko                 |");
  Serial.println("|                       www.vazduhgradjanima.rs                           |");
  Serial.print("|                       Firmware Version: ");
  Serial.print(firmwareVersion);
  Serial.println("                           |");
  Serial.println("|  Write 'config' to configure your credentials (expires in 10 seconds)   |");
  Serial.println(" -------------------------------------------------------------------------");
  getCredentials();
  while(millis() < 14000) {
    configureCredentials();
  }
  credentialsSDK();
  device.debugPort(Serial);
  device.wifiSignalReporting(true);
  device.connectionLed(true);
  device.setActuationCallback(INTERVAL_ASSET, controlInterval);
  device.createAsset("firmware", "Firmware Version", "sensor", "string"); // Create asset on AllThingsTalk for firmware version
  device.init();
  device.send("firmware", firmwareVersion); // Send the current firmware version to newly created AllThingsTalk asset
  publishInterval(); // Send current reporting interval
  pm1.begin();
  pm25.begin();
  pm10.begin();
  temp.begin();
  hum.begin();
  pres.begin();
  Serial.println("");
  Serial.println(">>>>>>>>>>>>>>>>>>  Your Klimerko is up and running!  <<<<<<<<<<<<<<<<<<<<<");
  Serial.print("Sensor data is read every ");
  Serial.print(readIntervalSeconds());
  Serial.print(" seconds (for averaging) and published every ");
  Serial.print(sendInterval);
  Serial.println(" minutes.");
  Serial.println("You can change the sending interval from your AllThingsTalk Maker.");
  Serial.println("Sensor data read interval dynamically adjusts based on the sending interval.");
  Serial.println("Average values are sent to the cloud - You can observe current sensor data in this window.");
  Serial.println("");
}

// Function to read both sensors at the same time
void readSensors() {
  // Check if it's time to wake up PMS7003
  if (millis() - lastReadTime >= readIntervalMillis() - (wakeInterval * 1000) && !pmsWoken) {
    Serial.println("System: Now waking up PMS7003");
    pmsPower(true);
  }
  
  // Read sensor data
  if (millis() - lastReadTime >= readIntervalMillis()) {
    Serial.println("------------------------------DATA------------------------------");
    readPMS();
    readBME();
    Serial.println("----------------------------------------------------------------");
    if (!noSleep) {
      Serial.print("System: Air Quality Sensor will sleep until ");
      Serial.print(wakeInterval);
      Serial.println(" seconds before next reading.");
      pmsPower(false);
    }
    lastReadTime = millis();
  }
  
  // Send average sensor data
  if (millis() - lastSendTime >= sendInterval * 60000) {
    Serial.println("Now sending averaged data to AllThingsTalk...");
    payload.reset();
    payload.set(AQ_ASSET, airQuality);
    payload.set(PM1_ASSET, avgPM1);
    payload.set(PM2_5_ASSET, avgPM25);
    payload.set(PM10_ASSET, avgPM10);
    payload.set(TEMPERATURE_ASSET, avgTemperature);
    payload.set(HUMIDITY_ASSET, avgHumidity);
    payload.set(PRESSURE_ASSET, avgPressure);
    device.send(payload);
    lastSendTime = millis();
  }
}

// Function that reads data from the PMS7003
void readPMS() {
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
  } else {
    Serial.println("System Error: Air Quality Sensor (PMS7003) returned no data on data request this time.");
  }
}

// Function for reading data from the BME280 Sensor
void readBME() {
  // Save the current BME280 sensor data
  // BME280 Heats itself up about 1-2 degrees and the NodeMCU
  // inside the Klimerko's 3D case interferes by about 2 degrees
  // Because of this, we'll subtract 4 degrees from the raw reading
  // (do note that the calibration/testing was done at 20*-25* celsius)
  float temperatureRaw =  bme.readTemperature() - 4;
  float humidityRaw = bme.readHumidity();
  float pressureRaw = bme.readPressure() / 100.0F;

  if (temperatureRaw != 0 && humidityRaw != 0) {
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
  } else {
    Serial.print("System Error: Temperature/Humidity/Pressure Sensor (BME280) returned no data on data request this time.");
  }
}

void pmsPower(bool state) {
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
 
void controlInterval(int interval) {
  if (interval > 5 && interval <= 60) {
    sendInterval = interval;
    noSleep = false;
    Serial.print("System: Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("System: Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishInterval();
  } else if (interval <= 5) {
    sendInterval = 5;
    noSleep = true;
    pmsPower(true);
    Serial.println("System: Reporting interval set to 5 minutes (minimum).");
    Serial.print("System: Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("System: Note that this prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishInterval();
  } else if (interval > 60) {
    noSleep = false;
    Serial.print("System: Received command to set reporting interval to ");
    Serial.print(interval);
    Serial.println(" minutes but that exceeds the maximum.");
    Serial.print("System: Reverted back to ");
    Serial.print(sendInterval);
    Serial.println(" minutes.");
    Serial.print("System: Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishInterval();
  }
}

void publishInterval() {
  Serial.println("System: Publishing the actual device reporting interval to AllThingsTalk");
  device.send(INTERVAL_ASSET, sendInterval);
}

int readIntervalMillis() {
  int result = (sendInterval * 60000) / averageSamples;
  return result;
}

int readIntervalSeconds() {
  int result = (sendInterval * 60) / averageSamples;
  return result;
}

void getCredentials() {
  //Serial.println("System: Loading your credentials...");
  EEPROM.begin(EEPROMsize);
  wifiName = readData(wifiName_EEPROM_begin, wifiName_EEPROM_end);
  wifiPassword = readData(wifiPassword_EEPROM_begin, wifiPassword_EEPROM_end);
  deviceId = readData(deviceId_EEPROM_begin, deviceId_EEPROM_end);
  deviceToken = readData(deviceToken_EEPROM_begin, deviceToken_EEPROM_end);
  EEPROM.end();
  Serial.print("Device ID (AllThingsTalk): ");
  Serial.println(deviceId);

//  Serial.print("MEMORY: WiFi Name: ");
//  Serial.println(wifiName);
//  Serial.print("MEMORY: WiFi Password: ");
//  Serial.println(wifiPassword);
//  Serial.print("MEMORY: ATT Device ID: ");
//  Serial.println(deviceId);
//  Serial.print("MEMORY: ATT Device Token: ");
//  Serial.println(deviceToken);
}

String readData(int startAddress, int endAddress) {
  String output;
  for (int i = startAddress; i < endAddress; ++i) {
    if (char(EEPROM.read(i)) != ';') {
      output += String(char(EEPROM.read(i)));
    } else {
      i = endAddress;
    }
  }
  return output;
}

// Convert credentials for SDK readability
void credentialsSDK() {
  char *wifiNameChar, *wifiPasswordChar, *deviceIdChar, *deviceTokenChar;
  wifiNameChar = (char*)malloc(wifiName.length()+1);
  wifiPasswordChar = (char*)malloc(wifiPassword.length()+1);
  deviceIdChar = (char*)malloc(deviceId.length()+1);
  deviceTokenChar = (char*)malloc(deviceToken.length()+1);
  
  wifiName.toCharArray(wifiNameChar, wifiName.length()+1);
  wifiPassword.toCharArray(wifiPasswordChar, wifiPassword.length()+1);
  deviceId.toCharArray(deviceIdChar, deviceId.length()+1);
  deviceToken.toCharArray(deviceTokenChar, deviceToken.length()+1);
  
  wifiCreds = WifiCredentials(wifiNameChar, wifiPasswordChar);
  deviceCreds = DeviceConfig(deviceIdChar, deviceTokenChar);
  device = Device(wifiCreds, deviceCreds);
}

void configureCredentials() {
  if (Serial.available() >= 1) {
    String cmd = Serial.readString();
    cmd.trim();
    if (cmd == "config") {
      Serial.println("");
      Serial.println("----------------------CREDENTIALS CONFIGURATION MODE----------------------");
      Serial.println("            To only change WiFi Credentials, enter 'wifi'");
      Serial.println("         To only change AllThingsTalk Credentials, enter 'att'");
      Serial.println("                    To change both, enter 'all'");
      Serial.println("     Enter anything else to exit Credentials configuration mode...");
      Serial.println("--------------------------------------------------------------------------");
      delay(1000);

      while (Serial.available()==0) {}
      String input = Serial.readString();
      input.trim();
      if (input == "wifi") {
        wifiNameTemp = newCredentials("Enter your new WiFi Name (SSID)");
        wifiPasswordTemp = newCredentials("Enter your new WiFi Password");
        promptSave(input);
      } else if (input == "att") {
        deviceIdTemp = newCredentials("Enter your new Device ID");
        deviceTokenTemp = newCredentials("Enter your new Device Token");
        promptSave(input);
      } else if (input == "all") {
        wifiNameTemp = newCredentials("Enter your new WiFi Name (SSID)");
        wifiPasswordTemp = newCredentials("Enter your new WiFi Password");
        deviceIdTemp = newCredentials("Enter your new Device ID");
        deviceTokenTemp = newCredentials("Enter your new Device Token");
        promptSave(input);
      } else {
        Serial.println("System: Exited Credentials Configuration Mode");
      }
    } else {
      Serial.println("Command not recognized. If you wish to configure your device, enter 'config'");
    }
  }
}

String newCredentials(String inputMessage) {
  Serial.print(inputMessage);
  Serial.print(": ");
  while (Serial.available()==0) {}
  String inputData = Serial.readString();
  inputData.trim();
  Serial.println(inputData);
  if (inputData.length() >= 255) {
    Serial.println("System ERROR: Your credentials are too long! Maximum is 255");
    restartDevice();
  } else if (inputData.indexOf(";") != -1) {
    Serial.print("System ERROR: Your credentials can't contain the '");
    Serial.print(dataDivider);
    Serial.println("' character");
    restartDevice();
  } else {
    return inputData;
  }
}

void promptSave(String input) {
  Serial.println("System: Save this configuration? Enter 'yes' to save and restart the device. Enter anything else to cancel.");
  while (Serial.available()==0) {}
  String confirm = Serial.readString();
  confirm.trim();
  if (confirm == "yes") {
    Serial.println("System: New credentials are being saved.");
    saveCredentials(input);
    restartDevice();
  } else {
    Serial.println("System: New credentials will NOT be saved. Normal operation resumed.");
  }
}

void saveCredentials(String whatToSave) {
  EEPROM.begin(EEPROMsize);
  whatToSave.trim();
  
  if (whatToSave == "wifi") {
    formatMemory(wifiName_EEPROM_begin, wifiPassword_EEPROM_end);
    writeData(wifiNameTemp, wifiName_EEPROM_begin);
    writeData(wifiPasswordTemp, wifiPassword_EEPROM_begin);
  }
  
  if (whatToSave == "att") {
    formatMemory(deviceId_EEPROM_begin, deviceToken_EEPROM_end);
    writeData(deviceIdTemp, deviceId_EEPROM_begin);
    writeData(deviceTokenTemp, deviceToken_EEPROM_begin);
  }

  if (whatToSave == "all") {
    formatMemory(wifiName_EEPROM_begin, deviceToken_EEPROM_end);
    writeData(wifiNameTemp, wifiName_EEPROM_begin);
    writeData(wifiPasswordTemp, wifiPassword_EEPROM_begin);
    writeData(deviceIdTemp, deviceId_EEPROM_begin);
    writeData(deviceTokenTemp, deviceToken_EEPROM_begin);
  }

  if (EEPROM.commit()) {
    Serial.println("System: Data saved.");
    EEPROM.end();
  } else {
    Serial.println("System Error: Data couldn't be saved.");
  }
}

void writeData(String data, int startAddress) {
//  Serial.print("MEMORY: Writing '");
//  Serial.print(data);
//  Serial.print("' (which is ");
//  Serial.print(data.length());
//  Serial.print(" bytes) from address ");
//  Serial.print(startAddress);
//  Serial.print(" so it ends at address ");
//  Serial.println(data.length() + startAddress);
  data = data+dataDivider;
  int characterNum = 0;
  for (int i = startAddress; i < (startAddress + data.length()); i++) {
    EEPROM.write(i, data[characterNum]);
    characterNum++;
  }
}

void formatMemory(int startAddress, int endAddress) {
    for (int i=startAddress; i < endAddress; i++) {
      EEPROM.write(i, ';');
    }
}

void restartDevice() {
  Serial.print("System: Restarting Klimerko in ");
  Serial.print(deviceRestartWait);
  Serial.print(" seconds");
  for (int i = 0; i < deviceRestartWait; i++) {
    Serial.print(".");
    delay(1000);
  }
  ESP.restart();
}

void loop() {
  readSensors(); // Read data from sensors
  device.loop(); // Keep the connection to AllThingsTalk alive
  //configureCredentials(); // Enable credentials configuration from serial interface
}
