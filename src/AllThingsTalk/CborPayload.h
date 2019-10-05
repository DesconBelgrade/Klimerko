#ifndef CBOR_PAYLOAD_H_
#define CBOR_PAYLOAD_H_

#include "CborEncoder.h"
#include "Payload.h"
#include "GeoLocation.h"

#include <string.h>
#include <stdint.h>

class CborPayload : public Payload {
public:
    CborPayload(unsigned int capacity = 256);
    ~CborPayload();

    template<typename T> bool set(char *assetName, T value);

    bool setTimestamp(uint64_t timestamp);
    bool setLocation(GeoLocation location);

    virtual unsigned char* getBytes();
    virtual unsigned int getSize();
    virtual void reset();

private:
    unsigned char *buffer;
    CborStaticOutput *output;
    CborWriter *writer;

    bool hasTimestamp = false;
    bool hasLocation = false;
    unsigned int capacity;
    unsigned int assetCount = 0;
    uint64_t timestamp;
    GeoLocation location;

    template<typename T> void write(T value);
};

#endif
