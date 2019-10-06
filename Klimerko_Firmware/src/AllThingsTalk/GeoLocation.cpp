#include "GeoLocation.h"

GeoLocation::GeoLocation() {

}

GeoLocation::GeoLocation(float latitude, float longitude, float altitude)
{
	this->latitude = latitude;
	this->longitude = longitude;
	this->altitude = altitude;
    this->isAltitudeSet = true;
}

GeoLocation::GeoLocation(float latitude, float longitude)
{
	this->latitude = latitude;
	this->longitude = longitude;
    this->isAltitudeSet = false;
}

bool GeoLocation::hasAltitude() {
    return isAltitudeSet;
}
