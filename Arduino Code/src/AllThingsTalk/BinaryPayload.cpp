#include <string.h>
#include <stdint.h>
#include "Arduino.h"

#include "BinaryPayload.h"
#include "GeoLocation.h"

BinaryPayload::BinaryPayload(unsigned int capacity) {
    this->capacity = capacity;
    buffer = new unsigned char[capacity];
    uint16_t endianCheck = 0x1;
    littleEndian = ((char*)&endianCheck)[0] == 1;
    releaseBuffer = true;
}

BinaryPayload::BinaryPayload(unsigned char *buffer, unsigned int length, unsigned int capacity) {
    if (capacity < length) {
        capacity = length;
    }
    this->buffer = buffer;
    this->capacity = capacity;
    offset = length;
    uint16_t endianCheck = 0x1;
    littleEndian = ((char*)&endianCheck)[0] == 1;
    releaseBuffer = false;
}

BinaryPayload::~BinaryPayload() {
    if (releaseBuffer) {
        delete[] buffer;
    }
}

unsigned char *BinaryPayload::getBytes() {
    return buffer;
}

unsigned int BinaryPayload::getSize() {
	return offset;
}

void BinaryPayload::reset() {
	this->offset = 0;
}

template<typename T> bool BinaryPayload::add(T t) {
    auto *arr = static_cast<unsigned char*>(static_cast<void*>(&t));

    if (sizeof(t) + offset > capacity)
        return false;

    if (littleEndian) {
        for (int i = sizeof(t) - 1; i >= 0; --i) {
            buffer[offset++] = arr[i];
        }
    } else {
        for (int i = 0; i < sizeof(t); ++i) {
            buffer[offset++] = arr[i];
        }
    }

    return true;
}

template<> bool BinaryPayload::add<String>(String t) {
    if (t.length() + offset > capacity)
        return false;

    for (int i = 0; i < t.length(); ++i) {
        buffer[offset++] = t[i];
    }

    return true;
}

template<> bool BinaryPayload::add<const char*>(const char *t) {
    if (strlen(t) + offset > capacity)
        return false;

    for (int i = 0; i < strlen(t); ++i) {
        buffer[offset++] = t[i];
    }

    return true;
}

template<> bool BinaryPayload::add(char *t) {
    if (strlen(t) + offset > capacity)
        return false;

    for (int i = 0; i < strlen(t); ++i) {
        buffer[offset++] = t[i];
    }

    return true;
}

template<> bool BinaryPayload::add(GeoLocation location) {
    add(location.latitude);
    add(location.longitude);
    if (location.hasAltitude()) {
        add(location.altitude);
    }
}

template bool BinaryPayload::add(int t);
template bool BinaryPayload::add(bool t);
template bool BinaryPayload::add(float t);
template bool BinaryPayload::add(double t);
template bool BinaryPayload::add(GeoLocation location);
