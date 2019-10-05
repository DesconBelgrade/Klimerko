#ifndef PAYLOAD_H_
#define PAYLOAD_H_

class Payload {
public:
    virtual unsigned char* getBytes() = 0;
    virtual unsigned int getSize() = 0;
    virtual void reset() = 0;
};

#endif
