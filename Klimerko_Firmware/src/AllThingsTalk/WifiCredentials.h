#ifndef WIFI_CREDENTIALS_H_
#define WIFI_CREDENTIALS_H_

#include "Credentials.h"

class WifiCredentials : public Credentials {
public:
    WifiCredentials();
    WifiCredentials(const char *ssid, const char *password);

    void setSsid(const char *ssid);
    void setPassword(const char *password);

    virtual const char *getSsid();
    virtual const char *getPassword();

private:
    const char *ssid;
    const char *password;
};

#endif
