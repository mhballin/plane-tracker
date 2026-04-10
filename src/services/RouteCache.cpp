// src/services/RouteCache.cpp
#include "RouteCache.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config/Config.h"

// NVS namespace (max 15 chars)
static constexpr char NVS_NS[] = "route_cache";

// ICAO -> IATA mapping (must stay in sync with OpenSkyService kAirlineTable)
static const struct { const char* icao; const char* iata; } kIataMap[] = {
    { "AAL", "AA" }, { "DAL", "DL" }, { "UAL", "UA" }, { "SWA", "WN" },
    { "JBU", "B6" }, { "FDX", "FX" }, { "UPS", "5X" }, { "ASA", "AS" },
    { "FFT", "F9" }, { "NKS", "NK" },
};

RouteCache::RouteCache() {}

String RouteCache::toIataFlightNumber(const String& callsign) {
    if (callsign.length() < 4) return "";
    String prefix = callsign.substring(0, 3);
    for (const auto& e : kIataMap) {
        if (prefix == e.icao) {
            return String(e.iata) + callsign.substring(3);
        }
    }
    return "";
}

bool RouteCache::lookup(const String& callsign,
                         String& origin, String& destination,
                         String& originName, String& destinationName) {
    if (callsign.isEmpty()) return false;

    // Check NVS first
    prefs_.begin(NVS_NS, true);  // read-only
    String cached = prefs_.getString(callsign.c_str(), "");
    prefs_.end();

    if (cached.length() > 0) {
        // Format: "BOS|Boston Logan|LAX|Los Angeles Intl"
        int p1 = cached.indexOf('|');
        int p2 = cached.indexOf('|', p1 + 1);
        int p3 = cached.indexOf('|', p2 + 1);
        if (p1 > 0 && p2 > p1 && p3 > p2) {
            origin          = cached.substring(0, p1);
            originName      = cached.substring(p1 + 1, p2);
            destination     = cached.substring(p2 + 1, p3);
            destinationName = cached.substring(p3 + 1);
            return true;
        }
    }

    // Not in NVS -- try API
    String iataFlight = toIataFlightNumber(callsign);
    if (iataFlight.isEmpty()) return false;

    // Skip if key is placeholder
    String key = Config::AERODATABOX_API_KEY;
    if (key == "your-aerodatabox-api-key" || key.isEmpty()) return false;

    if (fetchFromApi(iataFlight, origin, destination, originName, destinationName)) {
        store(callsign, origin, destination, originName, destinationName);
        return true;
    }
    return false;
}

void RouteCache::store(const String& callsign,
                        const String& origin, const String& destination,
                        const String& originName, const String& destinationName) {
    // NVS key max length = 15 chars; guard against unexpectedly long callsigns
    if (callsign.length() > 15) {
        Serial.printf("[RouteCache] Callsign too long for NVS key: %s\n", callsign.c_str());
        return;
    }
    String value = origin + "|" + originName + "|" + destination + "|" + destinationName;
    prefs_.begin(NVS_NS, false);  // read-write
    prefs_.putString(callsign.c_str(), value);
    prefs_.end();
    Serial.printf("[RouteCache] Stored: %s -> %s|%s\n",
                  callsign.c_str(), origin.c_str(), destination.c_str());
}

bool RouteCache::fetchFromApi(const String& iataFlightNumber,
                               String& origin, String& destination,
                               String& originName, String& destinationName) {
    // Today's date in YYYY-MM-DD
    struct tm ti;
    if (!getLocalTime(&ti)) return false;
    char dateBuf[12];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &ti);

    String url = "https://aerodatabox.p.rapidapi.com/flights/number/";
    url += iataFlightNumber;
    url += "/";
    url += dateBuf;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    http.addHeader("x-rapidapi-key",  Config::AERODATABOX_API_KEY);
    http.addHeader("x-rapidapi-host", "aerodatabox.p.rapidapi.com");

    Serial.printf("[RouteCache] GET %s\n", url.c_str());
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[RouteCache] HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[RouteCache] JSON error: %s\n", err.c_str());
        return false;
    }

    // AeroDataBox returns an array; use first element
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        // Some responses are objects wrapped in array -- try root object
        if (!doc["departure"].isNull()) {
            origin          = doc["departure"]["airport"]["iata"].as<String>();
            originName      = doc["departure"]["airport"]["name"].as<String>();
            destination     = doc["arrival"]["airport"]["iata"].as<String>();
            destinationName = doc["arrival"]["airport"]["name"].as<String>();
        } else {
            return false;
        }
    } else {
        JsonObject flight = arr[0];
        origin          = flight["departure"]["airport"]["iata"].as<String>();
        originName      = flight["departure"]["airport"]["name"].as<String>();
        destination     = flight["arrival"]["airport"]["iata"].as<String>();
        destinationName = flight["arrival"]["airport"]["name"].as<String>();
    }

    if (origin.isEmpty() || destination.isEmpty()) return false;

    Serial.printf("[RouteCache] Resolved: %s -> %s (%s -> %s)\n",
                  origin.c_str(), destination.c_str(),
                  originName.c_str(), destinationName.c_str());
    return true;
}
