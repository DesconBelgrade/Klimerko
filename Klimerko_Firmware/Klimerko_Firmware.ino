/*  This is taken from https://github.com/DesconBelgrade/Klimerko
 *  Please head over there to read more about the project.
 * 
 *  --------------- Project "KLIMERKO" ---------------
 *  Citizen Air Quality measuring device with cloud monitoring, built at https://descon.me for the whole world.
 *  This is a continued effort from https://descon.me/2018/winning-product/
 *  Supported by ISOC (Internet Society, Belgrade Chapter) https://isoc.rs and Beogradska Otvorena Skola www.bos.rs
 *  Project is powered by IoT Cloud Services and SDK from AllThingsTalk // www.allthingstalk.com
 *  3D Case for the device designed and built by Dusan Nikic
 *  Device designed, coded and built by Vanja Stanic // www.vanjastanic.com
*/

#include "src/AllThingsTalk/AllThingsTalk.h"
#include "src/AdafruitUnifiedSensor/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include <SoftwareSerial.h>
#include <Wire.h>

// ----------- CUSTOMIZABLE PART BEGIN -----------

// Your WiFi and Device Credentials here
auto wifiCreds = WifiCredentials("WIFI_SSID","WIFI_PASSWORD");
auto deviceCreds = DeviceConfig("YOUR_DEVICE_ID","YOUR_DEVICE_TOKEN");
auto device = Device(wifiCreds, deviceCreds);

// ----------- CUSTOMIZABLE PART END -------------

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

// Other variables
int readInterval = 15; // [MINUTES] Default device reporting time
int wakeInterval = 30; // [SECONDS] Seconds to activate sensor before reading it
const char*   airQuality;
unsigned long lastReadTime;
unsigned long currentTime;
bool firstReading = true;
bool pmsWoken = false;

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
  Serial.println();
  Serial.println("Project 'KLIMERKO' - https://github.com/DesconBelgrade/Klimerko");
  device.debugPort(Serial);
  device.wifiSignalReporting(true);
  device.connectionLed(true);
  device.setActuationCallback(INTERVAL_ASSET, controlInterval);
  device.init();
  publishInterval();
  delay(1000);
  Serial.println("//// Your device is up and running! ////"
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
    if (PM2_5 <= 12) {
      airQuality = "Good";
    } else if (PM2_5 >= 13 && PM2_5 <= 35) {
      airQuality = "Moderate";
    } else if (PM2_5 >= 36 && PM2_5 <= 55) {
      airQuality = "Poor";
    } else if (PM2_5 >= 56 && PM2_5 <= 150) {
      airQuality = "Unhealthy";
    } else if (PM2_5 >= 151 && PM2_5 <= 250) {
      airQuality = "Very Unhealthy";
    } else if (PM2_5 > 250) {
      airQuality = "Hazardous";
    }

    // Print via SERIAL
    Serial.println("----------PMS7003-----------");
    Serial.print("Air Quality is ");
    Serial.println(airQuality);
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

void loop() {
  device.loop(); // Keep the connection to AllThingsTalk alive
  readSensors(); // Read data from sensors
}
