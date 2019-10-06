#ifndef BINARY_PAYLOAD_H_
#define BINARY_PAYLOAD_H_

#include "Payload.h"
#include <string.h>
#include <stdint.h>

class BinaryPayload : public Payload {
public:
    BinaryPayload(unsigned char *bytes, unsigned int length, unsigned int capacity = 0);
    BinaryPayload(unsigned int capacity = 51);

    ~BinaryPayload();

    template<typename T> bool add(T t);

    virtual unsigned char* getBytes();
    virtual unsigned int getSize();
    virtual void reset();

private:
    unsigned char *buffer = NULL;
    unsigned int offset = 0;
    unsigned int capacity;

    bool releaseBuffer = true;
    bool littleEndian;
};

#endif
