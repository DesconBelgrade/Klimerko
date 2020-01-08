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

#include "AllThingsTalk_WiFi.h"
#include "Arduino.h"
#include "CborPayload.h"
#include "GeoLocation.h"
#include "BinaryPayload.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"

#ifdef ARDUINO_SAMD_MKRWIFI1010
#include "ATT_MKR1010.h"
#define SUPPORTS // Prevents error if no devices are supported by SDK
#endif

#ifdef ESP8266
#include "ATT_ESP8266.h"
#define SUPPORTS // Prevents error if no devices are supported by SDK
#endif

#ifndef SUPPORTS
Device::Device(WifiCredentials &wifiCreds, DeviceConfig &deviceCreds) {
    #error "Currently, ESP8266 (all ESP8266-based devices) and MKR1010 are supported. Open up an issue on GitHub if you'd like us to support your device."
}
#undef SUPPORTS
#endif
