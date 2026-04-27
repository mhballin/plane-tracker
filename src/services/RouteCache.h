// src/services/RouteCache.h
// NVS-backed route cache + AeroDataBox HTTP lookup.
// Routes are cached forever (airline routes don't change).
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <set>

class RouteCache {
public:
    RouteCache();

    /// Check NVS, then try hexdb.io → adsbdb → AeroDataBox.
    /// Returns true and populates all fields on success.
    /// origin/destination are IATA codes; city/country are human-readable.
    bool lookup(const String& callsign,
                String& origin, String& destination,
                String& originCity, String& originCountry,
                String& destinationCity, String& destinationCountry);

    /// Store a resolved route in NVS.
    void store(const String& callsign,
               const String& origin, const String& destination,
               const String& originCity, const String& originCountry,
               const String& destinationCity, const String& destinationCountry);

private:
    Preferences prefs_;
    std::set<String> notFound_;  // session-level negative cache — skip callsigns all backends returned 404

    /// Convert ICAO callsign prefix to IATA flight number (e.g. "UAL1234" -> "UA1234").
    String toIataFlightNumber(const String& callsign);

    /// HTTP fetch from hexdb.io (free, no key, best coverage). Returns true on success.
    bool fetchFromHexdb(const String& callsign,
                        String& origin, String& destination,
                        String& originCity, String& originCountry,
                        String& destinationCity, String& destinationCountry);

    /// HTTP fetch from adsbdb.com (free, no key, ICAO callsign direct). Returns true on success.
    bool fetchFromAdsbdb(const String& callsign,
                         String& origin, String& destination,
                         String& originCity, String& originCountry,
                         String& destinationCity, String& destinationCountry);

    /// HTTP fetch from AeroDataBox (paid fallback). Returns true on success.
    bool fetchFromApi(const String& iataFlightNumber,
                      String& origin, String& destination,
                      String& originCity, String& destinationCity);
};
