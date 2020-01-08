#ifndef DEVICECONFIG_H_
#define DEVICECONFIG_H_

#include "Credentials.h" 

class DeviceConfig : public Credentials {
public:
    DeviceConfig();
    DeviceConfig(const char *deviceId, const char* deviceToken);
    DeviceConfig(const char *deviceId, const char *deviceToken, const char *apiHostname);

    void setDeviceId(const char *deviceId);
    void setDeviceToken(const char *deviceToken);
    void setHostname(const char *hostname);

    virtual const char *getDeviceId();
    virtual const char *getDeviceToken();
    virtual const char *getHostname();

private:
    const char *deviceId;
    const char *deviceToken;
    const char *hostname = "api.allthingstalk.io";
};

#endif
