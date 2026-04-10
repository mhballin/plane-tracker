// src/services/RouteCache.h
// NVS-backed route cache + AeroDataBox HTTP lookup.
// Routes are cached forever (airline routes don't change).
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class RouteCache {
public:
    RouteCache();

    /// Check NVS, then fetch from AeroDataBox if not cached.
    /// Returns true and populates origin/destination on success.
    /// origin/destination are IATA airport codes (e.g. "BOS", "LAX").
    bool lookup(const String& callsign, String& origin, String& destination,
                String& originName, String& destinationName);

    /// Store a resolved route in NVS.
    void store(const String& callsign,
               const String& origin, const String& destination,
               const String& originName, const String& destinationName);

private:
    Preferences prefs_;

    /// Convert ICAO callsign prefix to IATA flight number (e.g. "UAL1234" -> "UA1234").
    String toIataFlightNumber(const String& callsign);

    /// HTTP fetch from AeroDataBox. Returns true on success.
    bool fetchFromApi(const String& iataFlightNumber,
                      String& origin, String& destination,
                      String& originName, String& destinationName);
};
