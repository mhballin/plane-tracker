// src/services/RouteCache.cpp
#include "RouteCache.h"
#include "data/AirlineTable.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config/Config.h"

RouteCache::RouteCache() {}

void RouteCache::setSDReady(bool ready) {
    sdReady_ = ready;
    if (ready) {
        Serial.println("[RouteCache] SD ready — routes persist to /routes.dat");
    } else {
        Serial.println("[RouteCache] No SD card — session RAM cache only");
    }
}

// ============================================================
// Shared field parser: "ORG|OrgCity|OrgCC|DST|DstCity|DstCC"
// ============================================================
static bool parseRouteFields(const String& data,
                              String& origin, String& originCity, String& originCountry,
                              String& destination, String& destinationCity, String& destinationCountry) {
    int p1 = data.indexOf('|');
    int p2 = data.indexOf('|', p1 + 1);
    int p3 = data.indexOf('|', p2 + 1);
    int p4 = data.indexOf('|', p3 + 1);
    int p5 = data.indexOf('|', p4 + 1);

    if (p1 <= 0) return false;

    if (p5 > p4) {
        origin             = data.substring(0, p1);
        originCity         = data.substring(p1 + 1, p2);
        originCountry      = data.substring(p2 + 1, p3);
        destination        = data.substring(p3 + 1, p4);
        destinationCity    = data.substring(p4 + 1, p5);
        destinationCountry = data.substring(p5 + 1);
    } else if (p3 > p2) {
        origin             = data.substring(0, p1);
        originCity         = data.substring(p1 + 1, p2);
        originCountry      = "";
        destination        = data.substring(p2 + 1, p3);
        destinationCity    = data.substring(p3 + 1);
        destinationCountry = "";
    } else {
        return false;
    }
    return !origin.isEmpty() && !destination.isEmpty();
}

// ============================================================
// SD helpers
// ============================================================

bool RouteCache::readRouteFromSD(const String& callsign,
                                  String& origin, String& destination,
                                  String& originCity, String& originCountry,
                                  String& destinationCity, String& destinationCountry) {
    if (!sdReady_) return false;
    File f = SD.open(ROUTES_FILE, FILE_READ);
    if (!f) return false;

    bool found = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        // Line format: CALLSIGN|ORG|OrgCity|OrgCC|DST|DstCity|DstCC[|YYYYMMDD]
        int p0 = line.indexOf('|');
        if (p0 <= 0 || line.substring(0, p0) != callsign) continue;

        String data = line.substring(p0 + 1);

        // Strip trailing date field if present (8 digits at end)
        int last = data.lastIndexOf('|');
        if (last > 0) {
            String tail = data.substring(last + 1);
            tail.trim();
            if (tail.length() == 8 && isdigit((unsigned char)tail[0])) {
                data = data.substring(0, last);
            }
        }

        found = parseRouteFields(data, origin, originCity, originCountry,
                                  destination, destinationCity, destinationCountry);
        break;
    }
    f.close();
    return found;
}

void RouteCache::writeRouteToSD(const String& callsign,
                                 const String& origin, const String& destination,
                                 const String& originCity, const String& originCountry,
                                 const String& destinationCity, const String& destinationCountry) {
    if (!sdReady_ || callsign.isEmpty() || origin.isEmpty() || destination.isEmpty()) return;

    char dateStr[9] = "";
    struct tm ti;
    if (getLocalTime(&ti) && ti.tm_year >= 120) {
        strftime(dateStr, sizeof(dateStr), "%Y%m%d", &ti);
    }

    File f = SD.open(ROUTES_FILE, FILE_APPEND);
    if (!f) { Serial.println("[RouteCache] SD route write failed"); return; }

    f.print(callsign);        f.print('|');
    f.print(origin);          f.print('|');
    f.print(originCity);      f.print('|');
    f.print(originCountry);   f.print('|');
    f.print(destination);     f.print('|');
    f.print(destinationCity); f.print('|');
    f.print(destinationCountry);
    if (strlen(dateStr) == 8) { f.print('|'); f.print(dateStr); }
    f.print('\n');
    f.close();
}

bool RouteCache::readTypeFromSD(const String& icao24, String& typeOut) {
    if (!sdReady_) return false;
    File f = SD.open(TYPES_FILE, FILE_READ);
    if (!f) return false;

    bool found = false;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        // Line format: ICAO24|Type description
        int p = line.indexOf('|');
        if (p <= 0 || line.substring(0, p) != icao24) continue;

        typeOut = line.substring(p + 1);
        typeOut.trim();
        found = !typeOut.isEmpty();
        break;
    }
    f.close();
    return found;
}

void RouteCache::writeTypeToSD(const String& icao24, const String& type) {
    if (!sdReady_ || icao24.isEmpty() || type.isEmpty()) return;
    File f = SD.open(TYPES_FILE, FILE_APPEND);
    if (!f) { Serial.println("[RouteCache] SD type write failed"); return; }
    f.print(icao24); f.print('|'); f.println(type);
    f.close();
}

// ============================================================
// IATA flight number conversion
// ============================================================
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

// ============================================================
// Aircraft type lookup: session RAM → SD → HTTP
// ============================================================
bool RouteCache::lookupType(const String& icao24, String& typeOut) {
    if (icao24.isEmpty()) return false;

    // 1. Session RAM
    auto it = typeCache_.find(icao24);
    if (it != typeCache_.end()) {
        typeOut = it->second;
        return !typeOut.isEmpty();
    }

    // 2. SD card (persists across reboots)
    if (readTypeFromSD(icao24, typeOut)) {
        typeCache_[icao24] = typeOut;
        return true;
    }

    if (Config::PERFORMANCE_CACHE_ONLY_ENRICHMENT) {
        // Keep UI smooth in production mode by avoiding blocking network lookups.
        typeCache_[icao24] = "";
        return false;
    }

    // 3. HTTP
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://hexdb.io/api/v1/aircraft/") + icao24);
    http.setTimeout(Config::DEBUG_LOOKUP_HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[RouteCache] type lookup failed (%d): %s\n", code, icao24.c_str());
        http.end();
        typeCache_[icao24] = "";
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) { typeCache_[icao24] = ""; return false; }

    String mfr  = doc["Manufacturer"].as<String>();
    String type = doc["Type"].as<String>();

    if (mfr.isEmpty() || mfr == "null") { typeCache_[icao24] = ""; return false; }

    typeOut = (type.isEmpty() || type == "null") ? mfr : mfr + " " + type;
    typeCache_[icao24] = typeOut;
    writeTypeToSD(icao24, typeOut);
    Serial.printf("[RouteCache] type: %s  %s\n", icao24.c_str(), typeOut.c_str());
    return true;
}

// ============================================================
// Route lookup: session RAM → SD → HTTP backends
// ============================================================
bool RouteCache::lookup(const String& callsign,
                         String& origin, String& destination,
                         String& originCity, String& originCountry,
                         String& destinationCity, String& destinationCountry) {
    if (callsign.isEmpty()) return false;
    if (notFound_.count(callsign)) return false;

    // 1. Session RAM
    auto hit = routeHits_.find(callsign);
    if (hit != routeHits_.end()) {
        origin             = hit->second.origin;
        destination        = hit->second.destination;
        originCity         = hit->second.originCity;
        originCountry      = hit->second.originCountry;
        destinationCity    = hit->second.destinationCity;
        destinationCountry = hit->second.destinationCountry;
        return true;
    }

    auto cacheAndReturn = [&](const char* tag) -> bool {
        routeHits_[callsign] = {origin, destination, originCity, originCountry,
                                 destinationCity, destinationCountry};
        Serial.printf("[RouteCache] route: %s  %s>%s (%s)\n",
                      callsign.c_str(), origin.c_str(), destination.c_str(), tag);
        return true;
    };

    // 2. SD card
    if (readRouteFromSD(callsign, origin, destination, originCity, originCountry,
                         destinationCity, destinationCountry)) {
        return cacheAndReturn("sd");
    }

    if (Config::PERFORMANCE_CACHE_ONLY_ENRICHMENT) {
        // Cache-only mode avoids long blocking HTTP fallback chains.
        notFound_.insert(callsign);
        return false;
    }

    // 3. HTTP backends
    if (fetchFromHexdb(callsign, origin, destination, originCity, originCountry,
                        destinationCity, destinationCountry)) {
        writeRouteToSD(callsign, origin, destination, originCity, originCountry,
                        destinationCity, destinationCountry);
        return cacheAndReturn("hexdb");
    }
    if (fetchFromAdsbdb(callsign, origin, destination, originCity, originCountry,
                         destinationCity, destinationCountry)) {
        writeRouteToSD(callsign, origin, destination, originCity, originCountry,
                        destinationCity, destinationCountry);
        return cacheAndReturn("adsbdb");
    }
    String iataFlight = toIataFlightNumber(callsign);
    if (!iataFlight.isEmpty()) {
        String key = Config::AERODATABOX_API_KEY;
        if (key != "your-aerodatabox-api-key" && !key.isEmpty()) {
            if (fetchFromApi(iataFlight, origin, destination, originCity, destinationCity)) {
                writeRouteToSD(callsign, origin, destination, originCity, "",
                                destinationCity, "");
                return cacheAndReturn("aerodatabox");
            }
        }
    }

    notFound_.insert(callsign);
    return false;
}

// ============================================================
// HTTP backend: hexdb.io
// ============================================================
bool RouteCache::fetchFromHexdb(const String& callsign,
                                  String& origin, String& destination,
                                  String& originCity, String& originCountry,
                                  String& destinationCity, String& destinationCountry) {
    String url = String("https://hexdb.io/callsign-route?callsign=") + callsign;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(Config::DEBUG_LOOKUP_HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;

    JsonObject fr = doc["flightroute"];
    if (fr.isNull()) return false;

    JsonObject org = fr["origin"];
    JsonObject dst = fr["destination"];
    if (org.isNull() || dst.isNull()) return false;

    origin             = org["iata_code"].as<String>();
    originCity         = org["municipality"].as<String>();
    originCountry      = org["country_iso_name"].as<String>();
    destination        = dst["iata_code"].as<String>();
    destinationCity    = dst["municipality"].as<String>();
    destinationCountry = dst["country_iso_name"].as<String>();

    if (origin.isEmpty() || origin == "null" || destination.isEmpty() || destination == "null") {
        return false;
    }
    return true;
}

// ============================================================
// HTTP backend: adsbdb.com
// ============================================================
bool RouteCache::fetchFromAdsbdb(const String& callsign,
                                  String& origin, String& destination,
                                  String& originCity, String& originCountry,
                                  String& destinationCity, String& destinationCountry) {
    String url = String("https://api.adsbdb.com/v0/callsign/") + callsign;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(Config::DEBUG_LOOKUP_HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    if (!doc["response"].is<JsonObject>()) return false;

    JsonObject fr = doc["response"]["flightroute"];
    if (fr.isNull()) return false;

    JsonObject org = fr["origin"];
    JsonObject dst = fr["destination"];
    if (org.isNull() || dst.isNull()) return false;

    origin             = org["iata_code"].as<String>();
    originCity         = org["municipality"].as<String>();
    originCountry      = org["country_iso_name"].as<String>();
    destination        = dst["iata_code"].as<String>();
    destinationCity    = dst["municipality"].as<String>();
    destinationCountry = dst["country_iso_name"].as<String>();

    if (origin.isEmpty() || origin == "null" || destination.isEmpty() || destination == "null") {
        return false;
    }
    return true;
}

// ============================================================
// HTTP backend: AeroDataBox (paid fallback)
// ============================================================
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
    http.setTimeout(Config::DEBUG_LOOKUP_HTTP_TIMEOUT_MS);
    http.addHeader("x-rapidapi-key",  Config::AERODATABOX_API_KEY);
    http.addHeader("x-rapidapi-host", "aerodatabox.p.rapidapi.com");

    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;

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
    return true;
}
