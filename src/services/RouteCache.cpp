// src/services/RouteCache.cpp
#include "RouteCache.h"
#include "data/AirlineTable.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config/Config.h"

// NVS namespace (max 15 chars)
static constexpr char NVS_NS[] = "route_cache";

RouteCache::RouteCache() {}

bool RouteCache::lookupType(const String& icao24, String& typeOut) {
    if (icao24.isEmpty()) return false;

    String url = String("https://hexdb.io/api/v1/aircraft/") + icao24;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    Serial.printf("[RouteCache] hexdb type GET %s\n", url.c_str());
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[RouteCache] hexdb type HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;

    String mfr  = doc["Manufacturer"].as<String>();
    String type = doc["Type"].as<String>();

    if (mfr.isEmpty() || mfr == "null") return false;

    typeOut = type.isEmpty() || type == "null" ? mfr : mfr + " " + type;
    Serial.printf("[RouteCache] type: %s\n", typeOut.c_str());
    return true;
}

String RouteCache::toIataFlightNumber(const String& callsign) {
    if (callsign.length() < 4) return "";
    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) {
            return String(entry.iataCode) + callsign.substring(3);
        }
    }
    return "";
}

bool RouteCache::lookup(const String& callsign,
                         String& origin, String& destination,
                         String& originCity, String& originCountry,
                         String& destinationCity, String& destinationCountry) {
    if (callsign.isEmpty()) return false;

    // Skip callsigns that returned 404 from all backends this session
    if (notFound_.count(callsign)) return false;

    // Check NVS first
    // New format (6 fields): "BOS|New York|US|LAX|Los Angeles|US"
    // Old format (4 fields): "BOS|Boston Logan|LAX|Los Angeles Intl"
    prefs_.begin(NVS_NS, true);
    String cached = prefs_.getString(callsign.c_str(), "");
    prefs_.end();

    if (cached.length() > 0) {
        int p1 = cached.indexOf('|');
        int p2 = cached.indexOf('|', p1 + 1);
        int p3 = cached.indexOf('|', p2 + 1);
        int p4 = cached.indexOf('|', p3 + 1);
        int p5 = cached.indexOf('|', p4 + 1);

        if (p5 > p4) {
            // New 6-field format
            origin          = cached.substring(0, p1);
            originCity      = cached.substring(p1 + 1, p2);
            originCountry   = cached.substring(p2 + 1, p3);
            destination     = cached.substring(p3 + 1, p4);
            destinationCity = cached.substring(p4 + 1, p5);
            destinationCountry = cached.substring(p5 + 1);
            return true;
        } else if (p3 > p2) {
            // Old 4-field format — still usable, just no country
            origin          = cached.substring(0, p1);
            originCity      = cached.substring(p1 + 1, p2);
            originCountry   = "";
            destination     = cached.substring(p2 + 1, p3);
            destinationCity = cached.substring(p3 + 1);
            destinationCountry = "";
            return true;
        }
    }

    // 1. hexdb.io (free, best coverage, returns full city+country)
    if (fetchFromHexdb(callsign, origin, destination, originCity, originCountry, destinationCity, destinationCountry)) {
        store(callsign, origin, destination, originCity, originCountry, destinationCity, destinationCountry);
        return true;
    }

    // 2. adsbdb.com (free fallback)
    if (fetchFromAdsbdb(callsign, origin, destination, originCity, originCountry, destinationCity, destinationCountry)) {
        store(callsign, origin, destination, originCity, originCountry, destinationCity, destinationCountry);
        return true;
    }

    // 3. AeroDataBox (paid fallback)
    String iataFlight = toIataFlightNumber(callsign);
    if (!iataFlight.isEmpty()) {
        String key = Config::AERODATABOX_API_KEY;
        if (key != "your-aerodatabox-api-key" && !key.isEmpty()) {
            if (fetchFromApi(iataFlight, origin, destination, originCity, destinationCity)) {
                store(callsign, origin, destination, originCity, "", destinationCity, "");
                return true;
            }
        }
    }

    notFound_.insert(callsign);
    return false;
}

void RouteCache::store(const String& callsign,
                        const String& origin, const String& destination,
                        const String& originCity, const String& originCountry,
                        const String& destinationCity, const String& destinationCountry) {
    if (callsign.length() > 15) {
        Serial.printf("[RouteCache] Callsign too long for NVS key: %s\n", callsign.c_str());
        return;
    }
    // New 6-field format: "BOS|New York|US|LAX|Los Angeles|US"
    String value = origin + "|" + originCity + "|" + originCountry + "|"
                 + destination + "|" + destinationCity + "|" + destinationCountry;
    prefs_.begin(NVS_NS, false);
    prefs_.putString(callsign.c_str(), value);
    prefs_.end();
    Serial.printf("[RouteCache] Stored: %s -> %s (%s, %s) -> %s (%s, %s)\n",
                  callsign.c_str(),
                  origin.c_str(), originCity.c_str(), originCountry.c_str(),
                  destination.c_str(), destinationCity.c_str(), destinationCountry.c_str());
}

// ========================================
// hexdb.io — free, best route coverage
// GET https://hexdb.io/callsign-route?callsign=BAW100
// Returns full airport objects with municipality + country_iso_name
// ========================================
bool RouteCache::fetchFromHexdb(const String& callsign,
                                  String& origin, String& destination,
                                  String& originCity, String& originCountry,
                                  String& destinationCity, String& destinationCountry) {
    String url = String("https://hexdb.io/callsign-route?callsign=") + callsign;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    Serial.printf("[RouteCache] hexdb GET %s\n", url.c_str());
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[RouteCache] hexdb HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[RouteCache] hexdb JSON error: %s\n", err.c_str());
        return false;
    }

    // hexdb response: { "callsign": "...", "flightroute": { "origin": {...}, "destination": {...} } }
    JsonObject fr = doc["flightroute"];
    if (fr.isNull()) {
        Serial.println("[RouteCache] hexdb: no flightroute in response");
        return false;
    }

    JsonObject org = fr["origin"];
    JsonObject dst = fr["destination"];
    if (org.isNull() || dst.isNull()) return false;

    origin          = org["iata_code"].as<String>();
    originCity      = org["municipality"].as<String>();
    originCountry   = org["country_iso_name"].as<String>();
    destination     = dst["iata_code"].as<String>();
    destinationCity = dst["municipality"].as<String>();
    destinationCountry = dst["country_iso_name"].as<String>();

    if (origin.isEmpty() || origin == "null" || destination.isEmpty() || destination == "null") {
        return false;
    }

    Serial.printf("[RouteCache] hexdb: %s (%s, %s) -> %s (%s, %s)\n",
                  origin.c_str(), originCity.c_str(), originCountry.c_str(),
                  destination.c_str(), destinationCity.c_str(), destinationCountry.c_str());
    return true;
}

// ========================================
// adsbdb.com fallback
// ========================================
bool RouteCache::fetchFromAdsbdb(const String& callsign,
                                  String& origin, String& destination,
                                  String& originCity, String& originCountry,
                                  String& destinationCity, String& destinationCountry) {
    String url = String("https://api.adsbdb.com/v0/callsign/") + callsign;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    Serial.printf("[RouteCache] adsbdb GET %s\n", url.c_str());
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[RouteCache] adsbdb HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[RouteCache] adsbdb JSON error: %s\n", err.c_str());
        return false;
    }

    if (!doc["response"].is<JsonObject>()) return false;

    JsonObject fr = doc["response"]["flightroute"];
    if (fr.isNull()) return false;

    JsonObject org = fr["origin"];
    JsonObject dst = fr["destination"];
    if (org.isNull() || dst.isNull()) return false;

    origin          = org["iata_code"].as<String>();
    originCity      = org["municipality"].as<String>();
    originCountry   = org["country_iso_name"].as<String>();
    destination     = dst["iata_code"].as<String>();
    destinationCity = dst["municipality"].as<String>();
    destinationCountry = dst["country_iso_name"].as<String>();

    if (origin.isEmpty() || origin == "null" || destination.isEmpty() || destination == "null") {
        return false;
    }

    Serial.printf("[RouteCache] adsbdb: %s (%s, %s) -> %s (%s, %s)\n",
                  origin.c_str(), originCity.c_str(), originCountry.c_str(),
                  destination.c_str(), destinationCity.c_str(), destinationCountry.c_str());
    return true;
}

// ========================================
// AeroDataBox paid fallback
// ========================================
bool RouteCache::fetchFromApi(const String& iataFlightNumber,
                               String& origin, String& destination,
                               String& originCity, String& destinationCity) {
    struct tm ti;
    if (!getLocalTime(&ti)) return false;
    char dateBuf[12];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);

    String url = "https://aerodatabox.p.rapidapi.com/flights/number/";
    url += iataFlightNumber;
    url += "/";
    url += dateBuf;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);
    http.addHeader("x-rapidapi-key",  Config::AERODATABOX_API_KEY);
    http.addHeader("x-rapidapi-host", "aerodatabox.p.rapidapi.com");

    Serial.printf("[RouteCache] AeroDataBox GET %s\n", url.c_str());
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[RouteCache] AeroDataBox HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[RouteCache] AeroDataBox JSON error: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        if (!doc["departure"].isNull()) {
            origin          = doc["departure"]["airport"]["iata"].as<String>();
            originCity      = doc["departure"]["airport"]["municipalityName"].as<String>();
            destination     = doc["arrival"]["airport"]["iata"].as<String>();
            destinationCity = doc["arrival"]["airport"]["municipalityName"].as<String>();
        } else {
            return false;
        }
    } else {
        JsonObject flight = arr[0];
        origin          = flight["departure"]["airport"]["iata"].as<String>();
        originCity      = flight["departure"]["airport"]["municipalityName"].as<String>();
        destination     = flight["arrival"]["airport"]["iata"].as<String>();
        destinationCity = flight["arrival"]["airport"]["municipalityName"].as<String>();
    }

    if (origin.isEmpty() || destination.isEmpty()) return false;

    Serial.printf("[RouteCache] AeroDataBox: %s (%s) -> %s (%s)\n",
                  origin.c_str(), originCity.c_str(),
                  destination.c_str(), destinationCity.c_str());
    return true;
}
