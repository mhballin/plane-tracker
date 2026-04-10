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
    float verticalRate;   // m/s, + = climbing
    String squawk;        // e.g. "1200"
    String aircraftType;
    String airline;
    String origin;
    String destination;
    bool onGround;
    bool valid;

    Aircraft() :
        latitude(0), longitude(0), altitude(0),
        velocity(0), heading(0), verticalRate(0),
        onGround(false), valid(false) {}
};
