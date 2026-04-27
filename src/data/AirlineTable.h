#pragma once
// src/data/AirlineTable.h
// Shared ICAO airline-prefix lookup table.
//
// Fields
//   prefix   — 3-letter ICAO operator designator (matches callsign[0..2])
//   airline  — human-readable carrier name
//   iataCode — 2-character IATA airline code used for flight-number conversion
//   cargo    — true for dedicated freighter operators
//
// Included by OpenSkyService.cpp and RouteCache.cpp.
// Anonymous namespace prevents ODR violations when the header is pulled into
// multiple translation units.

namespace {
    struct AirlineEntry {
        const char* prefix;
        const char* airline;
        const char* iataCode;
        bool        cargo;
    };

    static const AirlineEntry kAirlineTable[] = {
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
} // anonymous namespace
