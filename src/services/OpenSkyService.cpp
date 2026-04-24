// src/services/OpenSkyService.cpp
// Optimized OpenSky API service - faster and more reliable

#include "OpenSkyService.h"
#include <mbedtls/platform.h>
#include <esp_heap_caps.h>
using namespace Config;

namespace {
    constexpr uint8_t MAX_AUTH_ATTEMPTS = 1;   // auth server times out at 5s — don't retry and trash lwIP
    constexpr uint8_t MAX_HTTP_ATTEMPTS = 3;
    constexpr uint16_t RETRY_DELAY_MS = 500;
    constexpr uint16_t HTTP_TIMEOUT_MS = 12000;
}

// Lookup tables for callsign-prefix → airline/type guessing
// cargo=true marks freighters so guessAircraftType() can label them correctly
static const struct {
    const char* prefix;
    const char* airline;
    const char* iataCode;
    bool cargo;
} kAirlineTable[] = {
    // US Major
    { "AAL", "American Airlines",     "AA", false },
    { "DAL", "Delta Air Lines",       "DL", false },
    { "UAL", "United Airlines",       "UA", false },
    { "SWA", "Southwest Airlines",    "WN", false },
    { "JBU", "JetBlue",               "B6", false },
    { "ASA", "Alaska Airlines",       "AS", false },
    { "FFT", "Frontier Airlines",     "F9", false },
    { "NKS", "Spirit Airlines",       "NK", false },
    { "HAL", "Hawaiian Airlines",     "HA", false },
    { "SCX", "Sun Country Airlines",  "SY", false },
    // US Regional
    { "SKW", "SkyWest Airlines",      "OO", false },
    { "RPA", "Republic Airways",      "YX", false },
    { "ENY", "Envoy Air",             "MQ", false },
    { "PDT", "Piedmont Airlines",     "PT", false },
    { "PSA", "PSA Airlines",          "OH", false },
    { "QXE", "Horizon Air",           "QX", false },
    { "ASH", "Mesa Airlines",         "YV", false },
    { "KAP", "Cape Air",              "9K", false },
    { "GJS", "GoJet Airlines",        "G7", false },
    { "AWI", "Air Wisconsin",         "ZW", false },
    { "SLV", "Silver Airways",        "3M", false },
    { "VTE", "Contour Airlines",      "LF", false },
    // US Cargo
    { "FDX", "FedEx Express",         "FX", true  },
    { "UPS", "UPS Airlines",          "5X", true  },
    { "GTI", "Atlas Air",             "5Y", true  },
    { "ABX", "ABX Air",               "GB", true  },
    { "ATN", "Air Transport Intl",    "8C", true  },
    { "KFS", "Kalitta Air",           "K4", true  },
    { "PAC", "Polar Air Cargo",       "PO", true  },
    { "NCR", "National Air Cargo",    "N8", true  },
    { "AMF", "Amerijet",              "M6", true  },
    // Canada
    { "ACA", "Air Canada",            "AC", false },
    { "WJA", "WestJet",               "WS", false },
    { "TSC", "Air Transat",           "TS", false },
    { "PDM", "Porter Airlines",       "PD", false },
    { "JZA", "Air Canada Express",    "QK", false },
    { "SWG", "Sunwing Airlines",      "WG", false },
    { "CJT", "Cargojet",              "W8", true  },
    // Latin America & Caribbean
    { "AMX", "Aeromexico",            "AM", false },
    { "VOI", "Volaris",               "Y4", false },
    { "VIV", "VivaAerobus",           "VB", false },
    { "CMP", "Copa Airlines",         "CM", false },
    { "AVA", "Avianca",               "AV", false },
    { "LAN", "LATAM Airlines",        "LA", false },
    { "TAM", "LATAM Brasil",          "JJ", false },
    { "GLO", "Gol",                   "G3", false },
    { "AZU", "Azul Airlines",         "AD", false },
    { "BWA", "Caribbean Airlines",    "BW", false },
    { "BHS", "Bahamasair",            "UP", false },
    // Europe
    { "BAW", "British Airways",       "BA", false },
    { "VIR", "Virgin Atlantic",       "VS", false },
    { "DLH", "Lufthansa",             "LH", false },
    { "AFR", "Air France",            "AF", false },
    { "KLM", "KLM",                   "KL", false },
    { "IBE", "Iberia",                "IB", false },
    { "AZA", "ITA Airways",           "AZ", false },
    { "SAS", "Scandinavian Airlines", "SK", false },
    { "TAP", "TAP Air Portugal",      "TP", false },
    { "EIN", "Aer Lingus",            "EI", false },
    { "FIN", "Finnair",               "AY", false },
    { "LOT", "LOT Polish Airlines",   "LO", false },
    { "CSA", "Czech Airlines",        "OK", false },
    { "AUA", "Austrian Airlines",     "OS", false },
    { "SWR", "Swiss",                 "LX", false },
    { "ELY", "El Al",                 "LY", false },
    { "THY", "Turkish Airlines",      "TK", false },
    { "ICE", "Icelandair",            "FI", false },
    { "TRA", "Transavia",             "HV", false },
    { "WZZ", "Wizz Air",              "W6", false },
    { "EWG", "Eurowings",             "EW", false },
    { "BEL", "Brussels Airlines",     "SN", false },
    { "VLG", "Vueling",               "VY", false },
    { "NAX", "Norwegian Air",         "DY", false },
    { "RYR", "Ryanair",               "FR", false },
    { "EZY", "easyJet",               "U2", false },
    { "CLH", "Lufthansa CityLine",    "CL", false },
    { "AEE", "Aegean Airlines",       "A3", false },
    { "TOM", "TUI Airways",           "BY", false },
    { "EXS", "Jet2.com",              "LS", false },
    { "LOG", "Loganair",              "LM", false },
    { "VOE", "Volotea",               "V7", false },
    { "CLX", "Cargolux",              "CV", true  },
    // Middle East
    { "UAE", "Emirates",              "EK", false },
    { "ETD", "Etihad Airways",        "EY", false },
    { "QTR", "Qatar Airways",         "QR", false },
    { "SVA", "Saudia",                "SV", false },
    { "MSR", "EgyptAir",              "MS", false },
    { "RJA", "Royal Jordanian",       "RJ", false },
    { "MEA", "Middle East Airlines",  "ME", false },
    { "GFA", "Gulf Air",              "GF", false },
    { "OMA", "Oman Air",              "WY", false },
    { "FDB", "flydubai",              "FZ", false },
    { "ABY", "Air Arabia",            "G9", false },
    // Africa
    { "ETH", "Ethiopian Airlines",    "ET", false },
    { "KQA", "Kenya Airways",         "KQ", false },
    { "RAM", "Royal Air Maroc",       "AT", false },
    { "SAA", "South African Airways", "SA", false },
    // Asia-Pacific
    { "ANA", "All Nippon Airways",    "NH", false },
    { "JAL", "Japan Airlines",        "JL", false },
    { "CPA", "Cathay Pacific",        "CX", false },
    { "KAL", "Korean Air",            "KE", false },
    { "AAR", "Asiana Airlines",       "OZ", false },
    { "CES", "China Eastern",         "MU", false },
    { "CSN", "China Southern",        "CZ", false },
    { "CCA", "Air China",             "CA", false },
    { "EVA", "EVA Air",               "BR", false },
    { "CAL", "China Airlines",        "CI", false },
    { "MAS", "Malaysia Airlines",     "MH", false },
    { "SIA", "Singapore Airlines",    "SQ", false },
    { "THA", "Thai Airways",          "TG", false },
    { "VNA", "Vietnam Airlines",      "VN", false },
    { "PAL", "Philippine Airlines",   "PR", false },
    { "GIA", "Garuda Indonesia",      "GA", false },
    { "AIC", "Air India",             "AI", false },
    { "IGO", "IndiGo",                "6E", false },
    { "AXM", "AirAsia",               "AK", false },
    // Oceania
    { "QFA", "Qantas",                "QF", false },
    { "ANZ", "Air New Zealand",       "NZ", false },
    { "JST", "Jetstar",               "JQ", false },
    { "VAH", "Virgin Australia",      "VA", false },
};

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

    // Redirect mbedTLS allocations to PSRAM exactly once.
    // Default mbedTLS buffers are 2×16KB = 32KB of contiguous SRAM, which fails
    // (MBEDTLS_ERR_SSL_ALLOC_FAILED) after LVGL DMA buffers fragment the heap.
    // PSRAM has 8MB free and needs no DMA capability for SSL record buffers.
    static bool sslPsramInit = false;
    if (!sslPsramInit) {
        mbedtls_platform_set_calloc_free(
            [](size_t n, size_t size) -> void* {
                void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                return p ? p : calloc(n, size);  // fallback to SRAM if PSRAM full
            },
            free
        );
        sslPsramInit = true;
        Serial.printf("[SSL] mbedTLS → PSRAM  heap: %lu B  max: %lu B\n",
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getMaxAllocHeap());
    }

    for (uint8_t attempt = 1; attempt <= MAX_AUTH_ATTEMPTS; ++attempt) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.begin(client, OPENSKY_TOKEN_ENDPOINT);
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
        client.stop();

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
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.begin(client, url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Bearer " + accessToken);
        }

        Serial.println("[OpenSky] Fetching aircraft...");
        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            http.end();
            client.stop();

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                JsonArray states = doc["states"];
                if (!states.isNull()) {
                    for (JsonArray state : states) {
                        if (aircraftCount >= maxAircraft) break;
                        if (state[6].isNull() || state[5].isNull()) continue;

                        Aircraft &plane = aircraftList[aircraftCount];
                        plane.icao24    = state[0].isNull() ? "" : state[0].as<String>();
                        plane.callsign  = state[1].isNull() ? "" : state[1].as<String>();
                        plane.callsign.trim();
                        plane.longitude = state[5].as<float>();   // null-checked above
                        plane.latitude  = state[6].as<float>();   // null-checked above
                        plane.altitude  = state[7].isNull() ? 0.0f : state[7].as<float>();
                        // Default to false (airborne) when null — permissive, prefer showing over hiding aircraft
                        plane.onGround  = state[8].isNull() ? false : state[8].as<bool>();
                        if (plane.onGround) continue;
                        plane.velocity      = state[9].isNull()  ? 0.0f : state[9].as<float>();
                        plane.heading       = state[10].isNull() ? 0.0f : state[10].as<float>();
                        plane.verticalRate  = state[11].isNull() ? 0.0f : state[11].as<float>();
                        plane.squawk        = state[14].isNull() ? "" : state[14].as<String>();
                        plane.valid          = true;
                        plane.routeLookupDone    = false;
                        plane.origin             = "";
                        plane.destination        = "";
                        plane.originDisplay      = "";
                        plane.destinationDisplay = "";
                        plane.aircraftType = guessAircraftType(plane.callsign);
                        plane.airline      = guessAirline(plane.callsign);
                        if (plane.airline == "Private") continue;
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
            Serial.printf("[OpenSky] ❌ HTTP %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
        }

        http.end();
        client.stop();

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
    if (callsign.startsWith("N")) return "General Aviation";
    if (callsign.length() < 3) return "Aircraft";

    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) return entry.cargo ? "Cargo" : "Commercial Jet";
    }
    return "Unknown";
}

// ========================================
// Quick Airline Guess (from callsign)
// ========================================
String OpenSkyService::guessAirline(const String& callsign) {
    if (callsign.startsWith("N")) return "Private";
    if (callsign.length() < 3) return "Unknown";

    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) return entry.airline;
    }
    return "Unknown";
}

// ========================================
// IATA Flight Number (for AeroDataBox lookup)
// ========================================
String OpenSkyService::getIataFlightNumber(const String& callsign) {
    if (callsign.length() < 4) return "";
    String prefix = callsign.substring(0, 3);
    for (const auto& entry : kAirlineTable) {
        if (prefix == entry.prefix) {
            return String(entry.iataCode) + callsign.substring(3);
        }
    }
    return "";
}
