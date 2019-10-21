/*  This is taken from https://github.com/DesconBelgrade/Klimerko
 *  Please head over there to read more about the project.
 * 
 *  --------------- Project "KLIMERKO" ---------------
 *  Citizen Air Quality measuring device with cloud monitoring, built at https://descon.me for the whole world.
 *  Air Quality Scale (PM 10 based): Excellent (0-25), Good (26-35), Acceptable (36-50), Polluted (51-75), Very Polluted (Over 75)
 *  This is a continued effort from https://descon.me/2018/winning-product/
 *  Supported by ISOC (Internet Society, Belgrade Chapter) https://isoc.rs and Beogradska Otvorena Skola www.bos.rs
 *  Project is powered by IoT Cloud Services and SDK from AllThingsTalk // www.allthingstalk.com
 *  3D Case for the device designed and built by Dusan Nikic // nikic.dule@gmail.com
 *  Programmed and built by Vanja Stanic // www.vanjastanic.com
*/

#include "src/AllThingsTalk/AllThingsTalk.h"
#include "src/AdafruitBME280/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>

// Pins to which the PMS7003 is connected to
const uint8_t pmsTX = D5;
const uint8_t pmsRX = D6;

// Assets to be published to
char* PM1_ASSET         = "pm1";
char* PM2_5_ASSET       = "pm2-5";
char* PM10_ASSET        = "pm10";
char* AQ_ASSET          = "air-quality";
char* TEMPERATURE_ASSET = "temperature";
char* HUMIDITY_ASSET    = "humidity";
char* PRESSURE_ASSET    = "pressure";
char* INTERVAL_ASSET    = "interval";

// Other variables (don't touch if you don't know what you're doing)
int readInterval = 15; // [MINUTES] Default device reporting time
int wakeInterval = 30; // [SECONDS] Seconds to activate sensor before reading it
WifiCredentials wifiCreds = WifiCredentials("", "");
DeviceConfig deviceCreds = DeviceConfig("", "");
Device device = Device(wifiCreds, deviceCreds);
const char*   airQuality;
unsigned long lastReadTime;
unsigned long currentTime;
bool firstReading = true;
bool pmsWoken = false;
String wifiName, wifiPassword, deviceId, deviceToken;
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
int deviceRestartWait = 5; // Seconds

// Sensor initialization
Adafruit_BME280 bme;
SoftwareSerial pmsSerial(pmsTX, pmsRX);
PMS pms(pmsSerial);
PMS::DATA data;
CborPayload payload;

// Runs at boot
void setup() {
  Serial.begin(115200);
  pmsSerial.begin(9600);
  pms.passiveMode();
  bme.begin(0x76);
  Serial.println("");
  Serial.println("");
  Serial.println(" --------------------------Project 'KLIMERKO'-----------------------------");
  Serial.println("|              https://github.com/DesconBelgrade/Klimerko                 |");
  Serial.println("|                       www.vazduhgradjanima.rs                           |");
  Serial.println("|                        Firmware Version: 1.1                            |");
  Serial.println("|  Write 'config' to configure your credentials (expires in 10 seconds)   |");
  Serial.println(" -------------------------------------------------------------------------");
  getCredentials();
  while(millis() < 10000) {
    configureCredentials();
  }
  credentialsSDK();
  device.debugPort(Serial);
  device.wifiSignalReporting(true);
  device.connectionLed(true);
  device.setActuationCallback(INTERVAL_ASSET, controlInterval);
  device.init();
  publishInterval();
  delay(1000);
  Serial.println("//// Your device is up and running! ////");
  Serial.print("//// Sensor data will be read and published in ");
  Serial.print(readInterval);
  Serial.println(" minute(s)) ////");
  Serial.println("//// You can change this interval from AllThingsTalk Maker ////");
}

// Function to read both sensors at the same time
void readSensors() {
  currentTime = millis();
  if (currentTime - lastReadTime >= (readInterval * 60000) - (wakeInterval * 1000) && !pmsWoken) {
    Serial.println("System: Now waking up PMS7003");
    pms.wakeUp();
    pms.passiveMode();
    pmsWoken = true;
  }
  if (currentTime - lastReadTime >= readInterval * 60000) {
    payload.reset();
    readBME();
    readPMS();
    device.send(payload);
    lastReadTime = currentTime;
  }
}

// Function for reading data from the BME280 Sensor
void readBME() {
  // Save the current BME280 sensor data
  float temperatureRaw = bme.readTemperature();
  float humidityRaw = bme.readHumidity();
  float pressureRaw = bme.readPressure() / 100.0F;

  static char temperature[7];
  dtostrf(temperatureRaw, 6, 1, temperature);
  static char humidity[7];
  dtostrf(humidityRaw, 6, 1, humidity);
  static char pressure[7];
  dtostrf(pressureRaw, 6, 0, pressure);

  // Print data to Serial port
  Serial.println("-----------BME280-----------");
  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity:    ");
  Serial.println(humidity);
  Serial.print("Pressure:   ");
  Serial.println(pressure);
  Serial.println("----------------------------");

  // Add data to payload to be sent to AllThingsTalk
  payload.set(TEMPERATURE_ASSET, temperature);
  payload.set(HUMIDITY_ASSET, humidity);
  payload.set(PRESSURE_ASSET, pressure);
}

// Function that reads data from the PMS7003
void readPMS() {
  pms.requestRead();
  delay(1000);
  pms.requestRead();
  
  if (pms.readUntil(data)) {
    // Save the current Air Quality sensor data
    int PM1 = data.PM_AE_UG_1_0;
    int PM2_5 = data.PM_AE_UG_2_5;
    int PM10 = data.PM_AE_UG_10_0;


    // Assign a text value of how good the air is
    if (PM10 <= 25) {
      airQuality = "Excellent";
    } else if (PM10 >= 26 && PM10 <= 35) {
      airQuality = "Good";
    } else if (PM10 >= 36 && PM10 <= 50) {
      airQuality = "Acceptable";
    } else if (PM10 >= 51 && PM10 <= 75) {
      airQuality = "Polluted";
    } else if (PM10 > 76) {
      airQuality = "Very Polluted";
    }

    // Print via SERIAL
    Serial.println("----------PMS7003-----------");
    Serial.print("Air Quality is ");
    Serial.print(airQuality);
    Serial.println(", based on PM 10 value");
    Serial.print("PM 1.0 (ug/m3):  ");
    Serial.println(PM1);
    Serial.print("PM 2.5 (ug/m3):  ");
    Serial.println(PM2_5);
    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(PM10);
    Serial.println("----------------------------");

    // Add data to payload to be sent to AllThingsTalk
    payload.set(PM1_ASSET, PM1);
    payload.set(PM2_5_ASSET, PM2_5);
    payload.set(PM10_ASSET, PM10);
    payload.set(AQ_ASSET, airQuality);

    Serial.print("System: Putting PMS7003 to sleep until ");
    Serial.print(wakeInterval);
    Serial.println(" seconds before next reading.");
    pms.sleep();
    pmsWoken = false;
  } else {
    Serial.println("System Error: Air Quality Sensor (PMS7003) returned no data on data request this time.");
    pms.sleep();
    pmsWoken = false;
  }
}

// Function that's called once a user sets the interval 
void controlInterval(int interval) {
  if (interval != 0) {
    readInterval = interval;
    Serial.print("System: Device reporting interval changed to ");
    Serial.print(interval);
    Serial.println(" minute");
    publishInterval();
  } else {
    Serial.println("System Error: Attempted to set device reporting interval to 0. No changes made.");
    publishInterval();
  }
}

// Function (called at boot in setup()) that publishes the current data sending interval value to AllThingsTalk
void publishInterval() {
  Serial.println("System: Publishing the actual device reporting interval to AllThingsTalk");
  device.send(INTERVAL_ASSET, readInterval);
}

void getCredentials() {
  //Serial.println("System: Loading your credentials...");
  EEPROM.begin(EEPROMsize);
  wifiName = readData(wifiName_EEPROM_begin, wifiName_EEPROM_end);
  wifiPassword = readData(wifiPassword_EEPROM_begin, wifiPassword_EEPROM_end);
  deviceId = readData(deviceId_EEPROM_begin, deviceId_EEPROM_end);
  deviceToken = readData(deviceToken_EEPROM_begin, deviceToken_EEPROM_end);
  EEPROM.end();
//  Serial.print("WiFi Name: ");
//  Serial.println(wifiName);
//  Serial.print("WiFi Password: ");
//  Serial.println(wifiPassword);
//  Serial.print("ATT Device ID: ");
//  Serial.println(deviceId);
//  Serial.print("ATT Device Token: ");
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

// Convert credentials for SDK
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

// Main function
void configureCredentials() {
  if (Serial.available() >= 1) {
    if (Serial.readString() == "config\n") {
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
      if (input == "wifi\n") {
        wifiName = newCredentials("Enter your new WiFi Name (SSID)");
        wifiPassword = newCredentials("Enter your new WiFi Password");
        promptSave(input);
      } else if (input == "att\n") {
        deviceId = newCredentials("Enter your new Device ID");
        deviceToken = newCredentials("Enter your new Device Token");
        promptSave(input);
      } else if (input == "all\n") {
        wifiName = newCredentials("Enter your new WiFi Name (SSID)");
        wifiPassword = newCredentials("Enter your new WiFi Password");
        deviceId = newCredentials("Enter your new Device ID");
        deviceToken = newCredentials("Enter your new Device Token");
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
  if (Serial.readString() == "yes\n") {
    Serial.println("System: New credentials are being saved.");
    saveCredentials(input);
    restartDevice();
  } else {
    Serial.println("System: New credentials will NOT be saved. Normal operation resumed.");
  }
}

void saveCredentials(String whatToSave) {
  EEPROM.begin(EEPROMsize);
  
  if (whatToSave == "wifi\n") {
    formatMemory(wifiName_EEPROM_begin, wifiPassword_EEPROM_end);
    writeData(wifiName, wifiName_EEPROM_begin);
    writeData(wifiPassword, wifiPassword_EEPROM_begin);
  }
  
  if (whatToSave == "att\n") {
    formatMemory(deviceId_EEPROM_begin, deviceToken_EEPROM_end);
    writeData(deviceId, deviceId_EEPROM_begin);
    writeData(deviceToken, deviceToken_EEPROM_begin);
  }

  if (whatToSave == "all\n") {
    formatMemory(wifiName_EEPROM_begin, deviceToken_EEPROM_end);
    writeData(wifiName, wifiName_EEPROM_begin);
    writeData(wifiPassword, wifiPassword_EEPROM_begin);
    writeData(deviceId, deviceId_EEPROM_begin);
    writeData(deviceToken, deviceToken_EEPROM_begin);
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
  device.loop(); // Keep the connection to AllThingsTalk alive
  readSensors(); // Read data from sensors
  configureCredentials(); // Enable credentials configuration from serial interface
}
