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
 * Detailed instructions for this library can be found on https://github.com/allthingstalk/arduino-sdk-v2
 */
 
#ifndef ALLTHINGSTALK_H_
#define ALLTHINGSTALK_H_

#include "WifiCredentials.h"
#include "DeviceConfig.h"
#include "CborPayload.h"
#include "BinaryPayload.h"

class ActuationCallback {
public:
    String asset;
    void *actuationCallback;
    int actuationCallbackArgumentType;
    //void execute(JsonVariant variant);
};


class Device {
public:
    Device(WifiCredentials &wifiCreds, DeviceConfig &deviceCreds);
    // Initialization
    void init();
    
    // Maintaining Connection
    void loop();
    
    // Debug
    void debugPort(Stream &debugSerial);
    void debugPort(Stream &debugSerial, bool verbose);
    
    // Sending Data
    void send(CborPayload &payload);
    void send(BinaryPayload &payload);
    template<typename T> void send(char *asset, T payload);
    
    // Connection
    void connect();
    void disconnect();
    void connectWiFi();
    void setHostname(String hostname);
    void disconnectWiFi();
    void connectAllThingsTalk();
    void disconnectAllThingsTalk();
    
    // Connection LED
    void connectionLed(bool);
    void connectionLed(int ledPin);
    void connectionLed(bool state, int ledPin);
    
    // WiFi Signal Reporting
    void wifiSignalReporting(bool);
    void wifiSignalReporting(int time);
    void wifiSignalReporting(bool state, int time);
    String wifiSignal();
    
    // Callbacks (Receiving Data)
    bool setActuationCallback(String asset, void (*actuationCallback)(bool payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(int payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(double payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(float payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(const char* payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(String payload));

private:
    WifiCredentials *wifiCreds;
    DeviceConfig *deviceCreds;
    Stream *debugSerial;
    template<typename T> void debug(T message, char separator = '\n');
    template<typename T> void debugVerbose(T message, char separator = '\n');
    void generateRandomID();
    void fadeLed();
    void fadeLedStop();
    void maintainWiFi();
    void maintainAllThingsTalk();
    void reportWiFiSignal();
    void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length);
    
    // Actuations / Callbacks
    static const int maximumActuations = 32;
    ActuationCallback actuationCallbacks[maximumActuations];
    int actuationCallbackCount = 0;
    bool tryAddActuationCallback(String asset, void *actuationCallback, int actuationCallbackArgumentType);
    ActuationCallback *getActuationCallbackForAsset(String asset);
    
    // Connection Signal LED Parameters
    #define UP 1
    #define DOWN 0
    bool _ledEnabled                  = true;    // Default state for Connection LED
    int _connectionLedPin             = 2;       // Default Connection LED Pin for ESP8266
    static const int _minPWM          = 0;       // Minimum PWM
    static const int _maxPWM          = 1023;    // Maximum PWM
    static const byte _fadeIncrement  = 5;       // How smooth to fade
    static const int _fadeInterval    = 3;       // Interval between fading steps
    byte _fadeDirection               = UP;      // Initial value
    int _fadeValue                    = 1023;    // Initial value
    unsigned long _previousFadeMillis;           // millis() timing Variable, just for fading

    // MQTT Parameters
    char _mqttId[256];                      // Variable for saving generated client ID
    bool _callbackEnabled = true;           // Variable for checking if callback is enabled

    // WiFi Signal Reporting Parameters
    char* _wifiSignalAsset   = "wifi-signal"; // Asset name on AllThingsTalk for WiFi Signal Reporting
    bool _rssiReporting      = true;        // Default value for WiFi Signal Reporting
    int _rssiReportInterval  = 300;         // Default interval (seconds) for WiFi Signal Reporting
    unsigned long _rssiPrevTime;            // Remembers last time WiFi Signal was reported

    // Debug parameters
    bool _debugVerboseEnabled = false;

    // Connection parameters
    bool _disconnectedWiFi;                 // True when user intentionally disconnects
    bool _disconnectedAllThingsTalk;        // True when user intentionally disconnects
    String _wifiHostname;                   // WiFi Hostname itself
    bool _wifiHostnameSet = false;          // For tracking if WiFi hostname is set
};

#endif