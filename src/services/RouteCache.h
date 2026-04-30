// src/services/RouteCache.h
// SD-card-backed route + type cache with session RAM layer.
// Lookup order: session RAM → SD file → HTTP backends.
// SD files survive reboots and have no practical size limit.
#pragma once
#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <map>
#include <set>

class RouteCache {
public:
    RouteCache();

    // Call once after SD.begin() succeeds. Enables SD persistence.
    void setSDReady(bool ready);

    // Look up aircraft type by ICAO hex. Populates typeOut ("Boeing 737-800").
    bool lookupType(const String& icao24, String& typeOut);

    // Look up flight route by callsign.
    // Returns true and fills all fields on success.
    bool lookup(const String& callsign,
                String& origin, String& destination,
                String& originCity, String& originCountry,
                String& destinationCity, String& destinationCountry);

private:
    static constexpr const char* ROUTES_FILE = "/routes.dat";
    static constexpr const char* TYPES_FILE  = "/types.dat";

    struct RouteEntry {
        String origin, destination, originCity, originCountry, destinationCity, destinationCountry;
    };

    bool sdReady_ = false;

    std::set<String>             notFound_;   // session negative cache
    std::map<String, RouteEntry> routeHits_;  // session positive cache (callsign → route)
    std::map<String, String>     typeCache_;  // session positive cache (icao24 → type)

    // SD helpers
    bool readRouteFromSD(const String& callsign,
                         String& origin, String& destination,
                         String& originCity, String& originCountry,
                         String& destinationCity, String& destinationCountry);
    void writeRouteToSD(const String& callsign,
                        const String& origin, const String& destination,
                        const String& originCity, const String& originCountry,
                        const String& destinationCity, const String& destinationCountry);
    bool readTypeFromSD(const String& icao24, String& typeOut);
    void writeTypeToSD(const String& icao24, const String& type);

    String toIataFlightNumber(const String& callsign);

    bool fetchFromHexdb(const String& callsign,
                        String& origin, String& destination,
                        String& originCity, String& originCountry,
                        String& destinationCity, String& destinationCountry);
    bool fetchFromAdsbdb(const String& callsign,
                         String& origin, String& destination,
                         String& originCity, String& originCountry,
                         String& destinationCity, String& destinationCountry);
    bool fetchFromApi(const String& iataFlightNumber,
                      String& origin, String& destination,
                      String& originCity, String& destinationCity);
};
