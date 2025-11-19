// src/models/Aircraft.h

#pragma once
#include <Arduino.h>

class Aircraft {
public:
    String icao24;
    String callsign;
    float latitude;
    float longitude;
    float altitude;
    float velocity;
    float heading;
    String aircraftType;
    String airline;
    String origin;
    String destination;
    bool onGround;
    bool valid;

    Aircraft() : 
        latitude(0), longitude(0), altitude(0),
        velocity(0), heading(0), onGround(false), valid(false) {}
};