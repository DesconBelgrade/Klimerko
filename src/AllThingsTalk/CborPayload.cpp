#include <stdint.h>

#include "CborPayload.h"
#include "GeoLocation.h"

CborPayload::CborPayload(unsigned int capacity) {
    this->capacity = capacity;
    this->buffer = new unsigned char[capacity];
    reset();
}

CborPayload::~CborPayload() {
    delete[] buffer;
    delete output;
    delete writer;
}

void CborPayload::reset() {
    output = new CborStaticOutput(buffer, capacity);
    writer = new CborWriter(*output);
    assetCount = 0;

    // We're always assuming the full IoT Data Point (Tag 120)
    // is going to be used. The real state will be represented
    // only in getBytes().
    writer->writeTag(120);
    writer->writeArray(1);
    writer->writeMap(0);
}

bool CborPayload::setTimestamp(uint64_t timestamp) {
    hasTimestamp = true;
    this->timestamp = timestamp;
    return true;
}

bool CborPayload::setLocation(GeoLocation location) {
    hasLocation = true;
    this->location = location;
}

template<> void CborPayload::write(bool value) {
    writer->writeSpecial(20 + (value ? 1 : 0));
}

template<> void CborPayload::write(char *value) {
    writer->writeString(value);
}

template<> void CborPayload::write(const char *value) {
    writer->writeString(value);
}

template<> void CborPayload::write(String value) {
    char buffer[value.length() + 1];
    value.toCharArray(buffer, value.length() + 1);
    writer->writeString(buffer);
}

template<> void CborPayload::write(int value) {
    writer->writeInt(value);
}

template<> void CborPayload::write(float value) {
    writer->writeFloat(value);
}

template<> void CborPayload::write(double value) {
    writer->writeDouble(value);
}

template<> void CborPayload::write(GeoLocation location) {
    writer->writeTag(103);
    writer->writeArray(location.hasAltitude() ? 3 : 2);
    writer->writeFloat(location.latitude);
    writer->writeFloat(location.longitude);
    if (location.hasAltitude()) {
        writer->writeFloat(location.altitude);
    }
}

unsigned char *CborPayload::getBytes() {
    if (assetCount == 0) {
        return 0;
    }

    unsigned char meta = 1;
    if (hasTimestamp) meta = 2;
    if (hasLocation) meta = 3;

    auto headerOutput = CborStaticOutput(buffer, capacity);
    auto headerWriter = CborWriter(headerOutput);
    headerWriter.writeTag(120);
    headerWriter.writeArray(meta);
    headerWriter.writeMap(assetCount);

    auto footerOutput = CborStaticOutput(
        buffer + output->getSize(), capacity - output->getSize());
    auto footerWriter = CborWriter(footerOutput);

    if (hasTimestamp) {
        footerWriter.writeTag(1); // unix timestamp
        footerWriter.writeInt(timestamp);
    }

    if (hasLocation) {
        if (!hasTimestamp) footerWriter.writeSpecial(22); // null
        footerWriter.writeTag(103);
        footerWriter.writeArray(location.hasAltitude() ? 3 : 2);
        footerWriter.writeFloat(location.latitude);
        footerWriter.writeFloat(location.longitude);
        if (location.hasAltitude()) {
            footerWriter.writeFloat(location.altitude);
        }
    }

    if (meta > 1) {
        return buffer;
    } else {
        return buffer + 3;
    }
}

unsigned int CborPayload::getSize() {
    if (assetCount == 0) {
        return 0;
    }
    auto size = output->getSize();
    if (hasLocation) {
        size += 3 + 5 + 5; // geotag, latitude, longitude
        if (location.hasAltitude()) size += 5; // altitude
        if (!hasTimestamp) size += 1; // null for location
    }
    if (hasTimestamp) size += 6; // time tag, int
    if (!hasTimestamp && !hasLocation) size -= 3;

    return size;
}

template<typename T> bool CborPayload::set(char *assetName, T value) {
    writer->writeString(assetName);
    write(value);
    assetCount++;
}

template bool CborPayload::set(char *assetName, bool value);
template bool CborPayload::set(char *assetName, char *value);
template bool CborPayload::set(char *assetName, const char *value);
template bool CborPayload::set(char *assetName, String value);
template bool CborPayload::set(char *assetName, int value);
template bool CborPayload::set(char *assetName, float value);
template bool CborPayload::set(char *assetName, double value);
template bool CborPayload::set(char *assetName, GeoLocation value);
