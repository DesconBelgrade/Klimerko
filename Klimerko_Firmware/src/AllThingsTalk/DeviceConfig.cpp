#include "DeviceConfig.h"

DeviceConfig::DeviceConfig(const char *deviceId, const char *deviceToken) {
    setDeviceId(deviceId);
    setDeviceToken(deviceToken);
}

DeviceConfig::DeviceConfig(const char *deviceId, const char *deviceToken, const char *apiHostname) {
    setDeviceId(deviceId);
    setDeviceToken(deviceToken);
    setHostname(apiHostname);
}

void DeviceConfig::setDeviceId(const char *deviceId) {
    this->deviceId = deviceId;
}

void DeviceConfig::setDeviceToken(const char *deviceToken) {
    this->deviceToken = deviceToken;
}

void DeviceConfig::setHostname(const char *hostname) {
    this->hostname = hostname;
}

const char *DeviceConfig::getDeviceId() {
    return deviceId;
}

const char *DeviceConfig::getDeviceToken() {
    return deviceToken;
}

const char *DeviceConfig::getHostname() {
    return hostname;
}