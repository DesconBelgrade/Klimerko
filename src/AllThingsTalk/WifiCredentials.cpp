#include "WifiCredentials.h"

WifiCredentials::WifiCredentials(const char *ssid, const char *password) {
    setSsid(ssid);
    setPassword(password);
}

void WifiCredentials::setSsid(const char *ssid) {
    this->ssid = ssid;
}

void WifiCredentials::setPassword(const char *password) {
    this->password = password;
}

const char *WifiCredentials::getSsid() {
    return ssid;
}

const char *WifiCredentials::getPassword() {
    return password;
}