/*    _   _ _ _____ _    _              _____     _ _     ___ ___  _  __
 *   /_\ | | |_   _| |_ (_)_ _  __ _ __|_   _|_ _| | |__ / __|   \| |/ /
 *  / _ \| | | | | | ' \| | ' \/ _` (_-< | |/ _` | | / / \__ \ |) | ' <
 * /_/ \_\_|_| |_| |_||_|_|_||_\__, /__/ |_|\__,_|_|_\_\ |___/___/|_|\_\
 *                             |___/
 *
 * Copyright 2019 AllThingsTalk
 * Author: Vanja
 * https://allthingstalk.com
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This library makes connecting your Arduino devices with your AllThingsTalk Maker
 * a breeze, but it has quite a few more tricks up its sleeve.
 * Detailed instructions for this library can be found: https://github.com/allthingstalk/arduino-wifi-sdk
 */

#include <Ticker.h>
#include <ESP8266WiFi.h>

WiFiClient wifiClient;
PubSubClient client(wifiClient);
Ticker fader;

// Constructor
Device::Device(WifiCredentials &wifiCreds, DeviceConfig &deviceCreds) {
    this->deviceCreds = &deviceCreds;
    this->wifiCreds = &wifiCreds;
}

// Serial print (debugging)
template<typename T> void Device::debug(T message, char separator) {
    if (debugSerial) {
        debugSerial->print(message);
        if (separator) {
            debugSerial->print(separator);
        }
    }
}

// Serial print (verbose debugging)
template<typename T> void Device::debugVerbose(T message, char separator) {
    if (debugVerboseEnabled) {
        if (debugSerial) {
            debugSerial->print(message);
            if (separator) {
                debugSerial->print(separator);
            }
        }
    }
}

// Start fading the Connection LED
void Device::connectionLedFadeStart() {
    if (ledEnabled == true) {
        if (fader.active() == false) {
            fader.attach_ms(1, std::bind(&Device::connectionLedFadeStart, this));
        } else {
            unsigned long thisMillis = millis();
            if (thisMillis - previousFadeMillis >= fadeInterval) {
                if (fadeDirection == UP) {
                    fadeValue = fadeValue + fadeIncrement;
                    if (fadeValue >= maxPWM) {
                        fadeValue = maxPWM;
                        fadeDirection = DOWN;
                    }
            } else {
                fadeValue = fadeValue - fadeIncrement;
                if (fadeValue <= minPWM) {
                    fadeValue = minPWM;
                    fadeDirection = UP;
                }
            }
            analogWrite(connectionLedPin, fadeValue);
            previousFadeMillis = thisMillis;
            }
        }
    }
}

// Stop the Connection LED
void Device::connectionLedFadeStop() {
    if (ledEnabled == true) {
        fader.detach();
        while (fadeValue <= maxPWM) {
            delay(fadeInterval);
            fadeValue = fadeValue + fadeIncrement;
            analogWrite(connectionLedPin, fadeValue);
        }
        fadeValue = maxPWM;
        for (int i=0; i<=1; i++) {
            delay(100);
            analogWrite(connectionLedPin, minPWM);
            delay(50);
            analogWrite(connectionLedPin, maxPWM);
        }
    }
}

// Used to check if wifiSignalReporting is enabled
bool Device::wifiSignalReporting() {
    if (rssiReporting) {
        return true;
    } else {
        return false;
    }
}

// Used to set wifiSignalReporting on/off
bool Device::wifiSignalReporting(bool state) {
    rssiReporting = state;
    return true;
}

// Used to set wifiSignalReporting interval
bool Device::wifiSignalReporting(int time) {
    rssiReportInterval = time;
    return true;
}

// Used to set wifiSignalReporting on/off and interval in one go
bool Device::wifiSignalReporting(bool state, int time) {
    rssiReportInterval = time;
    rssiReporting = state;
    return true;
}

// Used to check if connectionLed is enabled or disabled
bool Device::connectionLed() {
    if (ledEnabled) {
        return true;
    } else {
        return false;
    }
}

// Used to set connectionLed on/off
bool Device::connectionLed(bool state) {
    ledEnabled = state;
    return true;
}

// Used to set a custom pin for connectionLed
bool Device::connectionLed(int ledPin) {
    connectionLedPin = ledPin;
    return true;
}

// Used to set connectionLed on/off and custom pin in one go
bool Device::connectionLed(bool state, int ledPin) {
    connectionLedPin = ledPin;
    ledEnabled = state;
    return true;
}

// Used to enable debug output
void Device::debugPort(Stream &debugSerial) {
    this->debugSerial = &debugSerial;
    debug("");
    debug("------------- AllThingsTalk WiFi SDK Serial Begin -------------");
    debug("Debug Level: Normal");
}

// Used to enable verbose debug output
void Device::debugPort(Stream &debugSerial, bool verbose) {
    debugVerboseEnabled = verbose;
    this->debugSerial = &debugSerial;
    debug("");
    debug("------------- AllThingsTalk WiFi SDK Serial Begin -------------");
    if (!verbose) debug("Debug Level: Normal");
    debugVerbose("Debug Level: Verbose");
    
}

// Generate Unique MQTT ID
void Device::generateRandomID() {
    sprintf(mqttId, "%s%i", "arduino-", ESP.getChipId());
    debugVerbose("Unique MQTT ID of Device:", ' ');
    debugVerbose(mqttId);
}

// Shows Device ID and Device Token via Serial in a hidden way (for visual verification)
void Device::showMaskedCredentials() {
    if (debugVerboseEnabled) {
        String hiddenDeviceId = deviceCreds->getDeviceId();
        String hiddenDeviceToken = deviceCreds->getDeviceToken();
        String lastFourDeviceId = hiddenDeviceId.substring(20);
        String lastFourDeviceToken = hiddenDeviceToken.substring(41);
        hiddenDeviceId = hiddenDeviceId.substring(0, 4);
        hiddenDeviceToken = hiddenDeviceToken.substring(0, 10);
        hiddenDeviceId += "****************";
        hiddenDeviceId += lastFourDeviceId;
        hiddenDeviceToken += "*******************************";
        hiddenDeviceToken += lastFourDeviceToken;
        debugVerbose("API Endpoint:", ' ');
        debugVerbose(deviceCreds->getHostname());
        debugVerbose("Device ID:", ' ');
        debugVerbose(hiddenDeviceId);
        debugVerbose("Device Token:", ' ');
        debugVerbose(hiddenDeviceToken);
    }
}

// Initialization of everything. Run in setup(), only after defining everything else.
void Device::init() {
    // Start flashing the Connection LED
    connectionLedFadeStart();
    
    // Print out information about Connection LED
    if (ledEnabled == false) {
        debug("Connection LED: Disabled");
    } else {
        debug("Connection LED: Enabled - GPIO", ' ');
        debug(connectionLedPin);
        debugVerbose("Please don't use GPIO", ' ');
        debugVerbose(connectionLedPin, ' ');
        debugVerbose("as it's used for Connection LED");
    }
    
    // Print out information about WiFi Signal Reporting
    if (rssiReporting == false) {
        debug("WiFi Signal Reporting: Disabled");
    } else {
        debug("WiFi Signal Reporting: Enabled");
        debugVerbose("WiFi Signal Strength will be published every", ' ');
        debugVerbose(rssiReportInterval, ' ');
        debugVerbose("seconds to 'wifi-signal' asset (will be created automatically)");
        createAsset("wifi-signal", "WiFi Signal Strength", "sensor", "string");
    }

    // Generate MQTT ID
    generateRandomID();

    // Print out the Device ID and Device Token in a hidden way (for visual verification)
    showMaskedCredentials();

    // Set MQTT Connection Parameters
    client.setServer(deviceCreds->getHostname(), 1883);
    if (callbackEnabled == true) {
        client.setCallback([this] (char* topic, byte* payload, unsigned int length) { this->mqttCallback(topic, payload, length); });
    }
    
    // Connect to WiFi and AllThingsTalk
    connectWiFi();
    createAssets();
    connectAllThingsTalk();
}

// Needs to be run in program loop in order to keep connections alive
void Device::loop() {
    maintainWiFi();
    client.loop();
    reportWiFiSignal();
    maintainAllThingsTalk();
}

// Connect to both WiFi and ATT
void Device::connect() {
    debug("Connecting to both WiFi and AllThingsTalk...");
    connectWiFi();
    connectAllThingsTalk();
}

// Disconnect both WiFi and ATT
void Device::disconnect() {
    debug("Disconnecting from both AllThingsTalk and WiFi...");
    disconnectAllThingsTalk();
    disconnectWiFi();
}

// Main method to connect to WiFi
void Device::connectWiFi() {
    if (!WiFi.localIP().isSet()) {
        connectionLedFadeStart();
        WiFi.mode(WIFI_STA);
        if (wifiHostnameSet) {
            WiFi.hostname(wifiHostname);
            debugVerbose("WiFi Hostname:", ' ');
            debugVerbose(wifiHostname);
        }
        debug("Connecting to WiFi:", ' ');
        debug(wifiCreds->getSsid(), '.');
        WiFi.begin(wifiCreds->getSsid(), wifiCreds->getPassword());
        while (!WiFi.localIP().isSet()) {
            debug("", '.');
            delay(10000);
        }
        debug("");
        debug("Connected to WiFi!");
        connectionLedFadeStop();
        debugVerbose("IP Address:", ' ');
        debugVerbose(WiFi.localIP());
        debugVerbose("WiFi Signal:", ' ');
        debugVerbose(wifiSignal());
        disconnectedWiFi = false;
    }
}

// Checks and recovers WiFi if lost
void Device::maintainWiFi() {
    if (!disconnectedWiFi) {
        if (!WiFi.localIP().isSet()) {
            connectionLedFadeStart();
            debug("WiFi Connection Dropped! Reason:", ' ');
            switch(WiFi.status()) {
                case 1:
                    debug("Seems like WiFi is connected, but it's giving an error (WL_CONNECTED)");
                    break;
                case 2:
                    debug("No WiFi Shield Present (WL_NO_SHIELD)");
                    break;
                case 3:
                    debug("WiFi.begin is currently trying to connect... (WL_IDLE_STATUS)");
                    break;
                case 4:
                    debug("SSID Not Available (WL_NO_SSID_AVAIL)");
                    break;
                case 5:
                    debug("WiFi Scan Completed (WL_SCAN_COMPLETED)");
                    break;
                case 6:
                    debug("Connection lost and failed to connect after many attempts (WL_CONNECT_FAILED)");
                    break;
                case 7:
                    debug("Connection Lost (WL_CONNECTION_LOST)");
                    break;
                case 8:
                    debug("Intentionally disconnected from the network (WL_DISCONNECTED)");
                    break;
                default:
                    debug("Unknown");
                    break;
            }
            connectWiFi();
            if (!disconnectedAllThingsTalk) {
                connectAllThingsTalk();
            }
        }
    }
}

// Main method for disconnecting from WiFi
void Device::disconnectWiFi() {
    if (WiFi.localIP().isSet()) {
        disconnectAllThingsTalk();
        WiFi.disconnect();
        disconnectedWiFi = true;
        while (WiFi.status() == WL_CONNECTED) {
            delay(1000);
        }
        debug("Successfully Disconnected from WiFi");
    }
}

// Used to set hostname that the device will present itself as on the network
bool Device::setHostname(String hostname) {
    wifiHostname = hostname;
    wifiHostnameSet = true;
    return true;
}

// Used to connect to HTTP (for asset creation)
bool Device::connectHttp() {
    if (!(wifiClient.connect(deviceCreds->getHostname(), 80))) {
        debug("Your Asset(s) can't be created on AllThingsTalk because the HTTP Connection failed.");
        return false;
    } else {
        return true;
    }
}

// Used to disconnect HTTP (for asset creation)
void Device::disconnectHttp() {
    wifiClient.flush();
    wifiClient.stop();
    debugVerbose("HTTP Connection (for Asset creation) to AllThingsTalk Closed");
}

// Used by the user to create a new asset on AllThingsTalk
bool Device::createAsset(String name, String title, String assetType, String dataType) {
    if (assetType == "sensor" || assetType == "actuator" || assetType == "virtual") {
    } else {
        String output = "Asset '" + name + "' (" + title + ") will not be created on AllThingTalk because it has an invalid asset type '" + assetType + "'.";
        debug(output);
        return false;
    }
    if (dataType == "boolean" || dataType == "string" || dataType == "integer" || dataType == "number" || dataType == "object" ||dataType == "array" ||  dataType == "location") {
    } else {
        String output = "Asset '" + name + "' (" + title + ") will not be created on AllThingTalk because it has an invalid data type '" + dataType + "'.";
        debug(output);
        return false;
    }
    
    assetsToCreate = true;
    assetProperties[assetsToCreateCount].name = name;
    assetProperties[assetsToCreateCount].title = title;
    assetProperties[assetsToCreateCount].assetType = assetType;
    assetProperties[assetsToCreateCount].dataType = dataType;
    ++assetsToCreateCount;
}

// Used by the SDK to actually create all the assets requested by user
AssetProperty *Device::createAssets() {
    if (assetsToCreate && connectHttp()) {
        connectionLedFadeStart();
        for (int i=0; i < assetsToCreateCount; i++) {
            connectHttp();
            wifiClient.println("PUT /device/" + String(deviceCreds->getDeviceId()) + "/asset/" + assetProperties[i].name  + " HTTP/1.1");
            wifiClient.print(F("Host: "));
            wifiClient.println(deviceCreds->getHostname());
            wifiClient.println(F("Content-Type: application/json"));
            wifiClient.print(F("Authorization: Bearer "));
            wifiClient.println(deviceCreds->getDeviceToken());
            wifiClient.print(F("Content-Length: ")); {
                int length = assetProperties[i].title.length() + assetProperties[i].dataType.length();
                
                if(assetProperties[i].assetType.equals("sensor")) {
                    length += 6;
                } else if(assetProperties[i].assetType.equals("actuator")) {
                    length += 8;
                } else if(assetProperties[i].assetType.equals("virtual")) {
                    length += 7;
                }

                if (assetProperties[i].dataType.length() == 0) {
                    length += 39;
                } else if (assetProperties[i].dataType[0] == '{') {
                    length += 49;
                } else {
                    length += 62;
                }
                wifiClient.println(length);
            }
            wifiClient.println();
            wifiClient.print(F("{\"title\":\""));
            wifiClient.print(assetProperties[i].title);
            wifiClient.print(F("\",\"is\":\""));
            wifiClient.print(assetProperties[i].assetType);
            if(assetProperties[i].dataType.length() == 0) {
                wifiClient.print(F("\""));
            } else if(assetProperties[i].dataType[0] == '{') {
                wifiClient.print(F("\",\"profile\": "));
                wifiClient.print(assetProperties[i].dataType);
            } else {
                wifiClient.print(F("\",\"profile\": { \"type\":\""));
                wifiClient.print(assetProperties[i].dataType);
                wifiClient.print(F("\" }"));
            }
            wifiClient.print(F("}"));
            wifiClient.println();
            wifiClient.println();
            
            unsigned long maxTime = millis() + 1000;
            while (millis() < maxTime) {
                if (wifiClient.available()) {
                    break;
                } else {
                    delay(10);
                }
            }
            
            if (debugVerboseEnabled) {
                if (wifiClient.available()) {
                    String response;
                    debugVerbose("---------------- HTTP Response from AllThingsTalk (Begin) ----------------");
                    while (wifiClient.available()) {
                        char c = wifiClient.read();
                        response += c;
                    }
                    debugVerbose(response);
                    debugVerbose("----------------- HTTP Response from AllThingsTalk (End) -----------------");
                }
            } else {
                if (wifiClient.available()) {
                    String output;
                    while (wifiClient.available()) { // This is of dubious value.
                        if (!wifiClient.find("HTTP/1.1")) {
                            break;
                        }
                        int responseCode = wifiClient.parseInt();
                        switch (responseCode) {
                            case 200:
                                output = "Updated existing " + assetProperties[i].dataType + " " + assetProperties[i].assetType + " asset '" + assetProperties[i].name + "' (" + assetProperties[i].title + ") on AllThingsTalk";
                                debug(output);
                                break;
                            case 201:
                                output = "Created a " + assetProperties[i].dataType + " " + assetProperties[i].assetType + " asset '" + assetProperties[i].name + "' (" + assetProperties[i].title + ") on AllThingsTalk";
                                debug(output);
                                break;
                            default:
                                output = "Failed to create a " + assetProperties[i].dataType + " " + assetProperties[i].assetType + " asset '" + assetProperties[i].name + "' (" + assetProperties[i].title + ") on AllThingsTalk. HTTP Response Code: " + responseCode;
                                debug(output);
                                break;
                        }
                        break;
                    }
                }
            }
        }
        disconnectHttp();
    }
}

// Connect to AllThingsTalk (and WiFi if it's disconnected)
void Device::connectAllThingsTalk() {
    if (!client.connected()) {
        connectionLedFadeStart();
        connectWiFi(); // WiFi needs to be present of course
        debug("Connecting to AllThingsTalk", '.');
        while (!client.connected()) {
            if (!WiFi.localIP().isSet()) {
                debug(" "); // Cosmetic only.
                maintainWiFi(); // In case WiFi connection is lost while connecting to ATT
            }
            if (!client.connected()) { // Double check while running to avoid double debug output (because maintainWiFi() also calls this method if ATT isn't connected)
                if (client.connect(mqttId, deviceCreds->getDeviceToken(), "arbitrary")) {
                    if (callbackEnabled == true) {
                        // Build the subscribe topic
                        char command_topic[256];
                        snprintf(command_topic, sizeof command_topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/asset/+/command");
                        client.subscribe(command_topic); // Subscribe to it
                    }
                    disconnectedAllThingsTalk = false;
                    debug("");
                    debug("Connected to AllThingsTalk!");
                    connectionLedFadeStop();
                    if (rssiReporting) send(wifiSignalAsset, wifiSignal()); // Send WiFi Signal Strength upon connecting
                } else {
                    debug("", '.');
                    delay(1500);
                }
            }                   
        }
    }
}

// Used to monitor AllThingsTalk connection and reconnect if dropped
void Device::maintainAllThingsTalk() {
    if (!disconnectedAllThingsTalk) {
        if (!client.connected()) {
            connectionLedFadeStart();
            debug("AllThingsTalk Connection Dropped! Reason:", ' ');
            switch (client.state()) {
                case -4:
                    debug("Server didn't respond within the keepalive time");
                    break;
                case -3:
                    debug("Network connection was broken");
                    break;
                case -2:
                    debug("Network connection failed.");
                    debugVerbose("This is a general error. Check if the asset you're publishing to exists on AllThingsTalk.");
                    break;
                case -1:
                    debug("Client disconnected cleanly (intentionally)");
                    break;
                case 0:
                    debug("Seems like client is connected. Restart device.");
                    break;
                case 1:
                    debug("Server doesn't support the requested protocol version");
                    break;
                case 2:
                    debug("Server rejected the client identifier");
                    break;
                case 3:
                    debug("Server was unable to accept the connection");
                    break;
                case 4:
                    debug("Bad username or password");
                    break;
                case 5:
                    debug("Client not authorized to connect");
                    break;
                default:
                    debug("Unknown");
                    break;
            }
            connectAllThingsTalk();
        }
    }
}

// Used to disconnect from AllThingsTalk
void Device::disconnectAllThingsTalk() {
    if (client.connected()) {
        client.disconnect();
        disconnectedAllThingsTalk = true;
        while (client.connected()) {}
        if (!client.connected()) {
            debug("Successfully Disconnected from AllThingsTalk");
        } else {
            debug("Failed to disconnect from AllThingsTalk");
        }
    }
}

// Called from loop; Reports wifi signal to ATTalk Maker at specified interval
void Device::reportWiFiSignal() {
    if (rssiReporting && WiFi.localIP().isSet()) {
        if (millis() - rssiPrevTime >= rssiReportInterval*1000) {
            send(wifiSignalAsset, wifiSignal());
            rssiPrevTime = millis();
        }
    }
}

// Reads wifi signal in RSSI and converts it to string
String Device::wifiSignal() {
    if (WiFi.status() == WL_CONNECTED) {
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
            debugVerbose("Error converting RSSI Values to WiFi Strength String");
        }
        return signalString;
    } else {
        debug("Can't read WiFi Signal Strength because you're not connected to WiFi");
    }
}

// Add boolean callback (0)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(bool payload)) {
    debugVerbose("Adding a Boolean Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 0);
}

// Add integer callback (1)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(int payload)) {
    debugVerbose("Adding an Integer Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 1);
}

// Add double callback (2)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(double payload)) {
    debugVerbose("Adding a Double Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 2);
}

// Add float callback (3)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(float payload)) {
    debugVerbose("Beware that the maximum value of float in 32-bit systems is 2,147,483,647");
    debugVerbose("Adding a Float Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 3);
}

// Add char callback (4)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(const char* payload)) {
    debugVerbose("Adding a Char Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 4);
}

// Add String callback (5)
bool Device::setActuationCallback(String asset, void (*actuationCallback)(String payload)) {
    debugVerbose("Adding a String Actuation Callback for Asset:", ' ');
    return tryAddActuationCallback(asset, (void*) actuationCallback, 5);
}

// Actual saving of added callbacks
bool Device::tryAddActuationCallback(String asset, void (*actuationCallback), int actuationCallbackArgumentType) {
   if (actuationCallbackCount >= maximumActuations) {
       debug("");
       debug("You've added too many actuations. The maximum is", ' ');
       debug(maximumActuations);
       return false;
   }
    callbackEnabled = true;
    actuationCallbacks[actuationCallbackCount].asset = asset;
    actuationCallbacks[actuationCallbackCount].actuationCallback = actuationCallback;
    actuationCallbacks[actuationCallbackCount].actuationCallbackArgumentType = actuationCallbackArgumentType;
    actuationCallbackCount++;
    debugVerbose(asset);
    return true;
}

// Retrieve a specific callback based on asset
ActuationCallback *Device::getActuationCallbackForAsset(String asset) {
    for (int i = 0; i < actuationCallbackCount; i++) {
        if (asset == actuationCallbacks[i].asset) {
            debugVerbose("Found Actuation Callback for Asset:", ' ');
            debugVerbose(actuationCallbacks[i].asset);
            return &actuationCallbacks[i];
        }
    }
    return nullptr;
}

// Asset name extraction from MQTT topic
String extractAssetNameFromTopic(String topic) {
    // Topic is formed as: device/ID/asset/NAME/state
    const int devicePrefixLength = 38;  // "device/ID/asset/"
    const int stateSuffixLength = 8;  // "/state"
    return topic.substring(devicePrefixLength, topic.length()-stateSuffixLength);
}

/* void ActuationCallback::execute(JsonVariant variant) {
}
*/

// MQTT Callback for receiving messages
void Device::mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length) {
    debugVerbose("--------------------------------------");
    debug("< Message Received from AllThingsTalk");
    String payload;
    String topic(p_topic);
    
    debugVerbose("Raw Topic:", ' ');
    debugVerbose(p_topic);

    // Whole JSON Payload
    for (uint8_t i = 0; i < p_length; i++) payload.concat((char)p_payload[i]);
    debugVerbose("Raw JSON Payload:", ' ');
    debugVerbose(payload);
    
    // Deserialize JSON
    DynamicJsonDocument doc(256);
    char json[256];
    for (int i = 0; i < p_length; i++) {
        json[i] = (char)p_payload[i];
    }
    auto error = deserializeJson(doc, json);
    if (error) {
        debug("Parsing JSON failed. Code:", ' ');
        debug(error.c_str());
        return;
    }

    // Extract time from JSON Document
    debugVerbose("Message Time:", ' ');
    const char* time = doc["at"];
    debugVerbose(time);

    String asset = extractAssetNameFromTopic(topic);
    debugVerbose("Asset Name:", ' ');
    debugVerbose(asset);

    // Call actuation callback for this specific asset
    ActuationCallback *actuationCallback = getActuationCallbackForAsset(asset);
    if (actuationCallback == nullptr) {
        debug("Error: There's no actuation callback for this asset.");
        return;
    }

    // Create JsonVariant which we'll use to to check data type and convert if necessary
    JsonVariant variant = doc["value"].as<JsonVariant>();

    //actuationCallback->execute(variant);


/*
    debug("------------------------------------------");
    if (variant.is<bool>()) debug("Value type: BOOLEAN");
    if (variant.is<int>()) debug("Value type: INTEGER");
    if (variant.is<double>()) debug("Value type: DOUBLE");
    if (variant.is<float>()) debug("Value type: FLOAT");
    if (variant.is<char*>()) debug("Value type: CHAR*");
    debug("------------------------------------------");
*/

    // BOOLEAN
    if (actuationCallback->actuationCallbackArgumentType == 0 && variant.is<bool>()) {
        bool value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: Boolean, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(bool payload)>(actuationCallback->actuationCallback)(value);
        return;
    }
    
    // INTEGER
    if (actuationCallback->actuationCallbackArgumentType == 1 && variant.is<int>() && variant.is<double>()) {
        int value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: Integer, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(int payload)>(actuationCallback->actuationCallback)(value);
        return;
    }

    // DOUBLE
    if (actuationCallback->actuationCallbackArgumentType == 2 && variant.is<double>()) {
        double value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: Double, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(double payload)>(actuationCallback->actuationCallback)(value);
        return;
    }

    // FLOAT
    if (actuationCallback->actuationCallbackArgumentType == 3 && variant.is<float>()) {
        float value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: Float, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(float payload)>(actuationCallback->actuationCallback)(value);
        return;
    }
    
    // CONST CHAR*
    if (actuationCallback->actuationCallbackArgumentType == 4 && variant.is<char*>()) {
        const char* value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: const char*, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(const char* payload)>(actuationCallback->actuationCallback)(value);
        return;
    }
    
    // STRING
    if (actuationCallback->actuationCallbackArgumentType == 5 && variant.is<char*>()) {
        String value = doc["value"];
        debugVerbose("Called Actuation for Asset:", ' ');
        debugVerbose(actuationCallback->asset, ',');
        debugVerbose(" Payload Type: String, Value:", ' ');
        debugVerbose(value);
        reinterpret_cast<void (*)(String payload)>(actuationCallback->actuationCallback)(value);
        return;
    }
    
    // JSON ARRAY
    if (variant.is<JsonArray>()) {
        debug("Receiving Arrays is not yet supported!");
        return;
    }
    
    // JSON OBJECT
    if (variant.is<JsonObject>()) {
        debug("Receiving Objects is not yet supported!");
        return;
    }
}

// void actuation(Location location) {
    // Serial.println(location.latitude);
// }

// Send data as CBOR
bool Device::send(CborPayload &payload) {
    if (WiFi.localIP().isSet()) {
        if (client.connected()) {
            char topic[128];
            snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/state");
            client.publish(topic, payload.getBytes(), payload.getSize());
            debug("> Message Published to AllThingsTalk (CBOR)");
            return true;
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
            return false;
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
        return false;
    }
}

// Send data as Binary Payload
bool Device::send(BinaryPayload &payload) {
    if (WiFi.localIP().isSet()) {
        if (client.connected()) {
            char topic[128];
            snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/state");
            client.publish(topic, payload.getBytes(), payload.getSize());
            debug("> Message Published to AllThingsTalk (Binary Payload)");
            return true;
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
            return false;
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
        return false;
    }
}

template<typename T> bool Device::send(char *asset, T payload) {
    if (WiFi.localIP().isSet()) {
        if (client.connected()) {
            char topic[128];
            snprintf(topic, sizeof topic, "%s%s%s%s%s", "device/", deviceCreds->getDeviceId(), "/asset/", asset, "/state");
            DynamicJsonDocument doc(256);
            char JSONmessageBuffer[256];
            doc["value"] = payload;
            serializeJson(doc, JSONmessageBuffer);
            client.publish(topic, JSONmessageBuffer, false);
            debug("> Message Published to AllThingsTalk (JSON)");
            debugVerbose("Asset:", ' ');
            debugVerbose(asset, ',');
            debugVerbose(" Value:", ' ');
            debugVerbose(payload);
            return true;
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
            return false;
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
        return false;
    }
}

template bool Device::send(char *asset, bool payload);
template bool Device::send(char *asset, char *payload);
template bool Device::send(char *asset, const char *payload);
template bool Device::send(char *asset, String payload);
template bool Device::send(char *asset, int payload);
template bool Device::send(char *asset, byte payload);
template bool Device::send(char *asset, short payload);
template bool Device::send(char *asset, long payload);
template bool Device::send(char *asset, float payload);
template bool Device::send(char *asset, double payload);
