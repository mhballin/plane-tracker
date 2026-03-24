// src/services/OpenSkyService.cpp
// Optimized OpenSky API service - faster and more reliable

#include "OpenSkyService.h"
using namespace Config;

namespace {
    constexpr uint8_t MAX_HTTP_ATTEMPTS = 3;
    constexpr uint16_t RETRY_DELAY_MS = 500; // base delay between retries (ms)
    constexpr uint16_t HTTP_TIMEOUT_MS = 12000;
}

OpenSkyService::OpenSkyService() : tokenExpiryTime(0), lastError("") {}

// ========================================
// OAuth2 Token Authentication
// ========================================
bool OpenSkyService::initialize() {
    Serial.println("[OpenSky] Requesting OAuth2 token...");
    return fetchAccessToken();
}

bool OpenSkyService::fetchAccessToken() {
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "OpenSky auth failed (WiFi disconnected)";
        Serial.println("[OpenSky] No WiFi connection");
        return false;
    }

    for (uint8_t attempt = 1; attempt <= MAX_HTTP_ATTEMPTS; ++attempt) {
        HTTPClient http;
        http.begin(OPENSKY_TOKEN_ENDPOINT);
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        char postData[512];
        snprintf(postData, sizeof(postData),
                 "grant_type=client_credentials&client_id=%s&client_secret=%s",
                 OPENSKY_CLIENT_ID, OPENSKY_CLIENT_SECRET);

        int httpCode = http.POST(postData);
        bool success = false;

        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc["access_token"]) {
                accessToken = doc["access_token"].as<String>();
                tokenExpiryTime = millis() + TOKEN_LIFETIME;
                lastError = "";
                Serial.println("[OpenSky] ✅ Authenticated successfully");
                success = true;
            } else {
                lastError = "OpenSky auth response parse error";
                Serial.println("[OpenSky] ❌ Failed to parse token response");
            }
        } else {
            lastError = String("OpenSky auth HTTP ") + httpCode;
            Serial.printf("[OpenSky] ❌ HTTP %d: %s\n", httpCode, http.getString().c_str());
        }

        http.end();

        if (success) {
            return true;
        }

        if (attempt < MAX_HTTP_ATTEMPTS) {
            delay(RETRY_DELAY_MS * attempt);
        }
    }

    return false;
}

// ========================================
// Fetch Aircraft (OPTIMIZED - No metadata lookups!)
// ========================================
int OpenSkyService::fetchAircraft(Aircraft* aircraftList, int maxAircraft) {
    static unsigned long lastApiCall = 0;
    const unsigned long API_RATE_LIMIT = 5000;  // 5 seconds between calls
    
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "OpenSky unavailable (WiFi disconnected)";
        Serial.println("[OpenSky] No WiFi connection");
        return 0;
    }
    
    // Check if token needs refresh
    if (millis() > tokenExpiryTime) {
        Serial.println("[OpenSky] Token expired, refreshing...");
        if (!fetchAccessToken()) {
            Serial.println("[OpenSky] Failed to refresh token");
            // Continue anyway - anonymous access might work
        }
    }
    
    // Rate limiting
    if (millis() - lastApiCall < API_RATE_LIMIT) {
        delay(API_RATE_LIMIT - (millis() - lastApiCall));
    }
    lastApiCall = millis();

    // Build URL with bounding box
    String url = "https://opensky-network.org/api/states/all";
    url += "?lamin=" + String(HOME_LAT - VISIBILITY_RANGE, 4);
    url += "&lomin=" + String(HOME_LON - VISIBILITY_RANGE, 4);
    url += "&lamax=" + String(HOME_LAT + VISIBILITY_RANGE, 4);
    url += "&lomax=" + String(HOME_LON + VISIBILITY_RANGE, 4);

    int aircraftCount = 0;

    for (uint8_t attempt = 1; attempt <= MAX_HTTP_ATTEMPTS; ++attempt) {
        HTTPClient http;
        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Bearer " + accessToken);
        }

        Serial.println("[OpenSky] Fetching aircraft...");
        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            http.end();

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                JsonArray states = doc["states"];
                if (!states.isNull()) {
                    for (JsonArray state : states) {
                        if (aircraftCount >= maxAircraft) break;
                        if (state[6].isNull() || state[5].isNull()) continue;

                        Aircraft &plane = aircraftList[aircraftCount];
                        plane.icao24 = state[0].as<String>();
                        plane.callsign = state[1].as<String>();
                        plane.callsign.trim();
                        plane.longitude = state[5].as<float>();
                        plane.latitude = state[6].as<float>();
                        plane.altitude = state[7].isNull() ? 0 : state[7].as<float>();
                        plane.onGround = state[8].as<bool>();
                        if (plane.onGround) continue;

                        plane.velocity = state[9].isNull() ? 0 : state[9].as<float>();
                        plane.heading = state[10].isNull() ? 0 : state[10].as<float>();
                        plane.valid = true;
                        plane.aircraftType = guessAircraftType(plane.callsign);
                        plane.airline = guessAirline(plane.callsign);
                        if (plane.airline == "Private") continue;

                        plane.origin = "";
                        plane.destination = "";
                        aircraftCount++;
                    }

                    if (aircraftCount == 0) {
                        lastError = "No aircraft in range";
                        Serial.println("[OpenSky] No aircraft detected in response");
                    } else {
                        lastError = "";
                        Serial.printf("[OpenSky] ✅ Found %d aircraft\n", aircraftCount);
                    }
                } else {
                    lastError = "OpenSky response missing states";
                    Serial.println("[OpenSky] No aircraft in response");
                }
            } else {
                lastError = "OpenSky JSON parse error";
                Serial.printf("[OpenSky] JSON parse error: %s\n", error.c_str());
            }

            return aircraftCount;
        } else {
            lastError = String("OpenSky HTTP ") + httpCode;
            Serial.printf("[OpenSky] ❌ HTTP %d\n", httpCode);
        }

        http.end();

        if (attempt < MAX_HTTP_ATTEMPTS) {
            delay(RETRY_DELAY_MS * attempt);
        }
    }

    return aircraftCount;
}

// ========================================
// Quick Aircraft Type Guess (from callsign)
// ========================================
String OpenSkyService::guessAircraftType(const String& callsign) {
    // Commercial airlines typically have 3-letter codes
    if (callsign.length() >= 3) {
        String prefix = callsign.substring(0, 3);
        
        // Check for common airline patterns
        if (prefix == "AAL" || prefix == "DAL" || prefix == "UAL" || 
            prefix == "SWA" || prefix == "JBU") {
            return "Commercial Jet";
        }
        
        // Cargo carriers
        if (prefix == "FDX" || prefix == "UPS") {
            return "Cargo Aircraft";
        }
    }
    
    // US registration numbers start with N
    if (callsign.startsWith("N")) {
        return "Private Aircraft";
    }
    
    return "Aircraft";
}

// ========================================
// Quick Airline Guess (from callsign)
// ========================================
String OpenSkyService::guessAirline(const String& callsign) {
    if (callsign.length() < 3) return "Unknown";
    
    String prefix = callsign.substring(0, 3);
    
    // Common US airlines
    if (prefix == "AAL") return "American Airlines";
    if (prefix == "DAL") return "Delta Air Lines";
    if (prefix == "UAL") return "United Airlines";
    if (prefix == "SWA") return "Southwest Airlines";
    if (prefix == "JBU") return "JetBlue Airways";
    if (prefix == "FDX") return "FedEx";
    if (prefix == "UPS") return "UPS Airlines";
    if (prefix == "ASA") return "Alaska Airlines";
    if (prefix == "FFT") return "Frontier Airlines";
    if (prefix == "NKS") return "Spirit Airlines";
    
    // Private aircraft
    if (callsign.startsWith("N")) return "Private";
    
    return "Airline";
}