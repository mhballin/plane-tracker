// src/services/OpenSkyService.h

#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../models/Aircraft.h"
#include "../config/Config.h"

class OpenSkyService {
private:
    String accessToken;
    unsigned long tokenExpiryTime;
    String lastError;
    
    // OAuth2 token management
    bool fetchAccessToken();
    
    // Quick aircraft identification (no slow API calls!)
    String guessAircraftType(const String& callsign);
    String guessAirline(const String& callsign);

public:
    OpenSkyService();
    
    // Initialize service and get OAuth token
    bool initialize();
    
    // Fetch aircraft in range (FAST - no metadata lookups!)
    int fetchAircraft(Aircraft* aircraftList, int maxAircraft);

    // Convert ICAO callsign prefix to IATA flight number (e.g. "AAL123" -> "AA123")
    String getIataFlightNumber(const String& callsign);

    const String& getLastError() const { return lastError; }
};