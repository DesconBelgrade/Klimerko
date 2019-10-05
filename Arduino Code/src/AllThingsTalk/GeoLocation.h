#ifndef GEO_LOCATION_H_
#define GEO_LOCATION_H_

class GeoLocation {

public:
    GeoLocation();

    GeoLocation(float latitude, float longitude, float altitude);
    GeoLocation(float latitude, float longitude);

    bool hasAltitude();

    float latitude = 0;
    float longitude = 0;
    float altitude = 0;

private:
    bool isAltitudeSet = false;
};

#endif
