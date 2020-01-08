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
 
#ifndef ALLTHINGSTALK_WIFI_H_
#define ALLTHINGSTALK_WIFI_H_

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

class AssetProperty {
public:
    String name;
    String title;
    String assetType;
    String dataType;
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
    
    // Create asset
    bool createAsset(String name, String title, String assetType, String dataType);
    
    // Sending Data
    bool send(CborPayload &payload);
    bool send(BinaryPayload &payload);
    template<typename T> bool send(char *asset, T payload);
    
    // Connection
    void connect();
    void disconnect();
    void connectWiFi();
    #ifdef ESP8266
    bool setHostname(String hostname);
    #else
    bool setHostname(const char* hostname);
    #endif
    void disconnectWiFi();
    void connectAllThingsTalk();
    void disconnectAllThingsTalk();
    
    // Connection LED
    bool connectionLed(); // Use to check if connection LED is enabled
    bool connectionLed(bool);
    bool connectionLed(int ledPin);
    bool connectionLed(bool state, int ledPin);
    
    // WiFi Signal Reporting
    bool wifiSignalReporting(); // Use to check if WiFi Signal Reporting is enabled
    bool wifiSignalReporting(bool);
    bool wifiSignalReporting(int time);
    bool wifiSignalReporting(bool state, int time);
    String wifiSignal();

    // Callbacks (Receiving Data)
    // These will return 
    bool setActuationCallback(String asset, void (*actuationCallback)(bool payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(int payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(double payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(float payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(const char* payload));
    bool setActuationCallback(String asset, void (*actuationCallback)(String payload));

private:
    WifiCredentials *wifiCreds;
    DeviceConfig *deviceCreds;
    
    // Debugging
    Stream *debugSerial;
    template<typename T> void debug(T message, char separator = '\n');
    template<typename T> void debugVerbose(T message, char separator = '\n');
    
    // Connection LED
    void connectionLedFadeStart();
    void connectionLedFadeStop();
    static void connectionLedFade();
    
    // Asset creation
    int assetsToCreateCount = 0;
    bool assetsToCreate = false;
    static const int maximumAssetsToCreate = 64;
    AssetProperty assetProperties[maximumAssetsToCreate];
    AssetProperty *createAssets();
    bool connectHttp();
    void disconnectHttp();
    
    // Connecting
    void generateRandomID();
    void maintainWiFi();
    void maintainAllThingsTalk();
    void reportWiFiSignal();
    void showMaskedCredentials();

    // Actuations / Callbacks
    #ifdef ESP8266
    void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length);
    #else
    static Device* instance; // // Internal callback saving for non-ESP devices (e.g. MKR)
    static void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length); // Static is only for MKR
    #endif
    static const int maximumActuations = 32;
    ActuationCallback actuationCallbacks[maximumActuations];
    int actuationCallbackCount = 0;
    bool tryAddActuationCallback(String asset, void *actuationCallback, int actuationCallbackArgumentType);
    ActuationCallback *getActuationCallbackForAsset(String asset);
    
    // Connection Signal LED Parameters
    #define UP 1
    #define DOWN 0
    #ifdef ESP8266
    bool ledEnabled                  = true;    // Default state for Connection LED
    int connectionLedPin             = 2;       // Default Connection LED Pin for ESP8266
    static const int minPWM          = 0;       // Minimum PWM
    static const int maxPWM          = 1023;    // Maximum PWM
    static const byte fadeIncrement  = 5;       // How smooth to fade
    static const int fadeInterval    = 3;       // Interval between fading steps
    byte fadeDirection               = UP;      // Initial value
    int fadeValue                    = 1023;    // Initial value
    unsigned long previousFadeMillis;           // millis() timing Variable, just for fading
    #else
    bool ledEnabled                  = true;    // Default state for Connection LED
    int connectionLedPin             = LED_BUILTIN;
    bool schedulerActive             = false;   // Keep track of scheduler
    bool supposedToFade              = false;   // Know if Connection LED is supposed to fade
    bool supposedToStop              = false;   // Keep track if fading is supposed to stop
    bool fadeOut                     = false;   // Keep track if Connection LED is supposed to fade out
    bool fadeOutBlink                = false;   // Keep track if Connection LED is supposed to blink after fading out
    static const int minPWM          = 0;       // Minimum PWM
    static const int maxPWM          = 255;     // Maximum PWM
    static const byte fadeIncrement  = 3;       // How smooth to fade
    static const int fadeInterval    = 10;      // Interval between fading steps
    static const int blinkInterval   = 100;     // Milliseconds between blinks at the end of connection led fade-out
    int fadeOutBlinkIteration        = 0;       // Keeps track of Connection LED blink iterations
    byte fadeDirection               = UP;      // Keep track which way should the LED Fade
    int fadeValue                    = 0;       // Keep track of current Connection LED brightness
    unsigned long previousFadeMillis;           // millis() timing Variable, just for fading
    unsigned long previousFadeOutMillis;        // Keeps track of time for fading out Connection LED
    unsigned long previousFadeOutBlinkMillis;   // Keeps track of time for blinks after fading out the LED
    #endif

    // MQTT Parameters
    char mqttId[32];                       // Variable for saving generated client ID
    bool callbackEnabled = true;           // Variable for checking if callback is enabled

    // WiFi Signal Reporting Parameters
    char* wifiSignalAsset   = "wifi-signal"; // Asset name on AllThingsTalk for WiFi Signal Reporting
    bool rssiReporting      = false;       // Default value for WiFi Signal Reporting
    int rssiReportInterval  = 300;         // Default interval (seconds) for WiFi Signal Reporting
    unsigned long rssiPrevTime;            // Remembers last time WiFi Signal was reported

    // Debug parameters
    bool debugVerboseEnabled = false;

    // Connection parameters
    bool disconnectedWiFi;                 // True when it's intentionally disconnected
    bool disconnectedAllThingsTalk;        // True when it's intentionally disconnected
    #ifdef ESP8266
    String wifiHostname;                   // WiFi Hostname itself
    #else
    const char* wifiHostname;              // Used for MKR
    #endif
    bool wifiHostnameSet = false;          // For tracking if WiFi hostname is set
};

#endif