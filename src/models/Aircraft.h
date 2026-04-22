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
    String origin;           // IATA code, e.g. "BOS"
    String destination;      // IATA code, e.g. "LAX"
    String originDisplay;    // city + country, e.g. "Boston, US"
    String destinationDisplay; // city + country, e.g. "Los Angeles, US"
    bool onGround;
    bool valid;
    bool routeLookupDone;   // true once a lookup attempt has completed (success or fail)

    Aircraft() :
        latitude(0), longitude(0), altitude(0),
        velocity(0), heading(0), verticalRate(0),
        squawk(""),
        onGround(false), valid(false), routeLookupDone(false) {}
};
