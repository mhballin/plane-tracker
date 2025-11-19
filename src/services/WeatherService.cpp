// src/services/WeatherService.cpp

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "../config/Config.h"
#include "WeatherService.h"

namespace {
    // Local constants
    constexpr int HTTP_OK_CODE = 200;
}

WeatherService::WeatherService(const String& apiKey, const String& city) 
    : apiKey(apiKey), city(city) {}

String WeatherService::buildUrl() const {
    String baseUrl = "http://api.openweathermap.org/data/2.5/weather?q=";
    return baseUrl + city + "&appid=" + apiKey + "&units=imperial";
}

String WeatherService::makeHttpRequest(const String& url) {
    String response;
    WiFiClient wifiClient;
    HTTPClient http;
    
    if (!http.begin(wifiClient, url.c_str())) {
        return response;
    }
    
    int httpCode = http.GET();
    if (httpCode == HTTP_OK_CODE) {
        response = http.getString();
    }
    
    http.end();
    return response;
}

bool WeatherService::parseWeatherData(const String& jsonData, WeatherData& weather) {
    // Use JsonDocument (DynamicJsonDocument is acceptable, keep size tuned)
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
        return false;
    }

    JsonObject root = doc.as<JsonObject>();
    // Prefer the is<T>() checks instead of deprecated containsKey
    if (!root["main"].is<JsonObject>() || !root["weather"].is<JsonArray>()) {
        return false;
    }

    JsonObject main = root["main"].as<JsonObject>();
    JsonArray weatherArray = root["weather"].as<JsonArray>();

    if (weatherArray.size() > 0) {
        weather.temperature = main["temp"] | 0.0f;
        weather.humidity = main["humidity"] | 0.0f;
        weather.pressure = main["pressure"] | 0.0f;
        weather.condition = weatherArray[0]["main"] | "Unknown";
        weather.description = weatherArray[0]["description"] | "No description";
        return true;
    }

    return false;
}

bool WeatherService::getWeather(WeatherData& weather) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    String url = buildUrl();
    String response = makeHttpRequest(url);
    
    if (response.length() == 0) {
        return false;
    }
    
    return parseWeatherData(response, weather);

}