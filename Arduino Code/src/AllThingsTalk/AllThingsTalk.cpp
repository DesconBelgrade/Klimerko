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
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This library makes connecting your Arduino devices with your AllThingsTalk Maker
 * a breeze, but it has quite a few more tricks up its sleeve.
 * Detailed instructions for this library can be found: https://github.com/allthingstalk/arduino-sdk-v2
 */

#include "AllThingsTalk.h"
#include "Arduino.h"
#include "CborPayload.h"
#include "GeoLocation.h"
#include "BinaryPayload.h"
#include "PubSubClient.h"
#include <ArduinoJson.h>

#ifdef ESP8266
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
    if (_debugVerboseEnabled) {
        if (debugSerial) {
            debugSerial->print(message);
            if (separator) {
                debugSerial->print(separator);
            }
        }
    }
}

// Connection Signal LED
void Device::fadeLed() {
    if (_ledEnabled == true) {
        if (fader.active() == false) {
            fader.attach_ms(1, std::bind(&Device::fadeLed, this));
        } else {
            unsigned long thisMillis = millis();
            if (thisMillis - _previousFadeMillis >= _fadeInterval) {
                if (_fadeDirection == UP) {
                    _fadeValue = _fadeValue + _fadeIncrement;
                    if (_fadeValue >= _maxPWM) {
                        _fadeValue = _maxPWM;
                        _fadeDirection = DOWN;
                    }
            } else {
                _fadeValue = _fadeValue - _fadeIncrement;
                if (_fadeValue <= _minPWM) {
                    _fadeValue = _minPWM;
                    _fadeDirection = UP;
                }
            }
            analogWrite(_connectionLedPin, _fadeValue);
            _previousFadeMillis = thisMillis;
            }
        }
    }
}

// Turn off Connection Signal LED
void Device::fadeLedStop() {
    if (_ledEnabled == true) {
        fader.detach();
        while (_fadeValue <= _maxPWM) {
            delay(_fadeInterval);
            _fadeValue = _fadeValue + _fadeIncrement;
            analogWrite(_connectionLedPin, _fadeValue);
        }
        _fadeValue = _maxPWM;
        for (int i=0; i<=1; i++) {
            delay(100);
            analogWrite(_connectionLedPin, _minPWM);
            delay(50);
            analogWrite(_connectionLedPin, _maxPWM);
        }
    }
}

void Device::wifiSignalReporting(bool state) {
    _rssiReporting = state;
}

void Device::wifiSignalReporting(int time) {
    _rssiReportInterval = time;
}

void Device::wifiSignalReporting(bool state, int time) {
    _rssiReportInterval = time;
    _rssiReporting = state;
}

void Device::connectionLed(bool state) {
    _ledEnabled = state;
}

void Device::connectionLed(int ledPin) {
    _connectionLedPin = ledPin;
}

void Device::connectionLed(bool state, int ledPin) {
    _connectionLedPin = ledPin;
    _ledEnabled = state;
}

void Device::debugPort(Stream &debugSerial) {
    this->debugSerial = &debugSerial;
    debug("");
    debug("------------- AllThingsTalk SDK Serial Begin -------------");
    debug("Debug Level: Normal");
}

void Device::debugPort(Stream &debugSerial, bool verbose) {
    _debugVerboseEnabled = verbose;
    this->debugSerial = &debugSerial;
    debug("");
    debug("------------- AllThingsTalk SDK Serial Begin -------------");
    if (!verbose) debug("Debug Level: Normal");
    debugVerbose("Debug Level: Verbose");
    
}

// Generate Random MQTT ID - If two same IDs are on one broker, the connection drops
void Device::generateRandomID() {
    randomSeed(analogRead(0));
    long randValue = random(2147483647);
    snprintf(_mqttId, sizeof _mqttId, "%s%dl", "arduino-", randValue);
    debugVerbose("Generated Unique ID for this Device:", ' ');
    debugVerbose("arduino", '-');
    debugVerbose(randValue);
}

// Initialization of everything. Run in setup(), only after defining everything else.
void Device::init() {
    // Start flashing the Connection LED
    fadeLed();
    
    // Print out information about Connection LED
    if (_ledEnabled == false) {
        debug("Connection LED: Disabled");
    } else {
        debug("Connection LED: Enabled - GPIO", ' ');
        debug(_connectionLedPin);
        debugVerbose("Please don't use GPIO", ' ');
        debugVerbose(_connectionLedPin, ' ');
        debugVerbose("as it's used for Connection LED");
    }
    
    // Print out information about WiFi Signal Reporting
    if (_rssiReporting == false) {
        debug("WiFi Signal Reporting: Disabled");
    } else {
        debug("WiFi Signal Reporting: Enabled");
        debugVerbose("WiFi Signal Strength will be published every", ' ');
        debugVerbose(_rssiReportInterval, ' ');
        debugVerbose("seconds");
        debugVerbose("To read WiFi Strength, create a Sensor asset 'wifi-signal' of type 'String' on your AllThingsTalk Maker");
    }

    // Generate MQTT ID
    generateRandomID();

    // Print out the Device ID and Device Token
    debugVerbose("API Endpoint:", ' ');
    debugVerbose(deviceCreds->getHostname());
    debugVerbose("Device ID:", ' ');
    debugVerbose(deviceCreds->getDeviceId());
    debugVerbose("Device Token:", ' ');
    debugVerbose(deviceCreds->getDeviceToken());

    // Set MQTT Connection Parameters
    client.setServer(deviceCreds->getHostname(), 1883);
    if (_callbackEnabled == true) {
        client.setCallback([this] (char* topic, byte* payload, unsigned int length) { this->mqttCallback(topic, payload, length); });
    }
    
    // Connect to WiFi and AllThingsTalk
    connect();
}

// Needs to be run in program loop in order to keep connections alive
void Device::loop() {
    if (!_disconnectedWiFi) {
        if (WiFi.status() != WL_CONNECTED) {
            maintainWiFi();
        } else {
            if (!_disconnectedAllThingsTalk) {
                if (!client.connected()) {
                    maintainAllThingsTalk();
                } else if (client.connected()) {
                    client.loop();
                    reportWiFiSignal();
                }
            }
        }
    }
}

void Device::connect() {
    debug("Connecting to both WiFi and AllThingsTalk...");
    connectWiFi();
    connectAllThingsTalk();
}

void Device::disconnect() {
    debug("Disconnecting from both AllThingsTalk and WiFi...");
    disconnectAllThingsTalk();
    disconnectWiFi();
}

void Device::connectWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        fadeLed();
        WiFi.mode(WIFI_STA);
        if (_wifiHostnameSet) {
            WiFi.hostname(_wifiHostname);
            debugVerbose("WiFi Hostname:", ' ');
            debugVerbose(_wifiHostname);
        }
        debug("Connecting to WiFi:", ' ');
        debug(wifiCreds->getSsid(), '.');
        WiFi.begin(wifiCreds->getSsid(), wifiCreds->getPassword());
        while (WiFi.status() != WL_CONNECTED) delay(1000); debug("", '.');
        debug("");
        debug("Connected to WiFi!");
        debugVerbose("IP Address:", ' ');
        debugVerbose(WiFi.localIP());
        debugVerbose("WiFi Signal:", ' ');
        debugVerbose(wifiSignal());
        _disconnectedWiFi = false;
    }
}

void Device::disconnectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        disconnectAllThingsTalk();
        WiFi.disconnect();
        _disconnectedWiFi = true;
        while (WiFi.status() == WL_CONNECTED) {}
        debug("Successfully Disconnected from WiFi");
    }
}

void Device::setHostname(String hostname) {
    _wifiHostname = hostname;
    _wifiHostnameSet = true;
}

void Device::connectAllThingsTalk() {
    if (!client.connected()) {
        fadeLed();
        connectWiFi();
        debug("Connecting to AllThingsTalk...");
        while (!client.connected()) {
            if (client.connect(_mqttId, deviceCreds->getDeviceToken(), "arbitrary")) {
                if (_callbackEnabled == true) {
                    // Build the subscribe topic
                    char command_topic[256];
                    snprintf(command_topic, sizeof command_topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/asset/+/command");
                    client.subscribe(command_topic);
                }
                _disconnectedAllThingsTalk = false;
                fadeLedStop();
                debug("Connected to AllThingsTalk!");
                if (_rssiReporting) send(_wifiSignalAsset, wifiSignal()); // Send WiFi Signal Strength upon connecting
            } else {
                debug("Failed to connect to AllThingsTalk. Reason:", ' ');
                switch (client.state()) {
                    case -4:
                        debug("Server didn't respond within the keepalive time");
                        break;
                    case -3:
                        debug("Network connection was broken");
                        break;
                    case -2:
                        debug("Network connection failed. This is a general error, but maybe check your credentials for AllThingsTalk");
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
                delay(5000);
            }
        }
    }
}

void Device::disconnectAllThingsTalk() {
    if (client.connected()) {
        client.disconnect();
        _disconnectedAllThingsTalk = true;
        while (client.connected()) {}
        debug("Successfully Disconnected from AllThingsTalk");
    }
}

void Device::maintainWiFi() {
    fadeLed();
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
    debug("Reconnecting to WiFi", ' ');
    while (WiFi.status() != WL_CONNECTED) {
        debug("", '.');
        delay(5000);
    }
    if (WiFi.status() == WL_CONNECTED) {
        debug("");
        debug("WiFi Connected!");
        debug("IP Address:", ' ');
        debug(WiFi.localIP(), ',');
        debug(" WiFi Signal:", ' ');
        debug(wifiSignal());
        fadeLedStop();
        _disconnectedWiFi = false;
    }
}

void Device::maintainAllThingsTalk() {
    while (!client.connected()) {
        connectAllThingsTalk();
    }
}

// Called from loop; Reports wifi signal to ATTalk Maker at specified interval
void Device::reportWiFiSignal() {
    if (_rssiReporting && WiFi.status() == WL_CONNECTED) {
        if (millis() - _rssiPrevTime >= _rssiReportInterval*1000) {
            send(_wifiSignalAsset, wifiSignal());
            _rssiPrevTime = millis();
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
    debugVerbose("Beware the maximum value of float in 32-bit systems is 2,147,483,647");
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
    _callbackEnabled = true;
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
void Device::send(CborPayload &payload) {
    if (WiFi.status() == WL_CONNECTED) {
        if (client.connected()) {
            char topic[128];
            snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/state");
            client.publish(topic, payload.getBytes(), payload.getSize());
            debug("> Message Published to AllThingsTalk (CBOR)");
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
    }
}

// Send data as Binary Payload
void Device::send(BinaryPayload &payload) {
    if (WiFi.status() == WL_CONNECTED) {
        if (client.connected()) {
            char topic[128];
            snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceCreds->getDeviceId(), "/state");
            client.publish(topic, payload.getBytes(), payload.getSize());
            debug("> Message Published to AllThingsTalk (Binary Payload)");
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
    }
}

template<typename T> void Device::send(char *asset, T payload) {
    if (WiFi.status() == WL_CONNECTED) {
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
        } else {
            debug("Can't publish message because you're not connected to AllThingsTalk");
        }
    } else {
        debug("Can't publish message because you're not connected to WiFi");
    }
}

template void Device::send(char *asset, bool payload);
template void Device::send(char *asset, char *payload);
template void Device::send(char *asset, const char *payload);
template void Device::send(char *asset, String payload);
template void Device::send(char *asset, int payload);
template void Device::send(char *asset, float payload);
template void Device::send(char *asset, double payload);

#define __HAS_IMPLEMENTATION // Prevents error if no devices are supported by SDK
#endif //(starts from #ifdef ARDUINO_ESP8266_NODEMCU)

#ifndef __HAS_IMPLEMENTATION
Device::Device(WifiCredentials &wifiCreds, DeviceConfig &deviceCreds) {
    #error "Currently, only ESP8266 and ESP8266 Dev boards are supported. This will change shortly."

}
#undef __HAS_IMPLEMENTATION
#endif
