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
    constexpr int HTTP_OK_CODE = 200;
    constexpr uint8_t MAX_HTTP_ATTEMPTS = 3;
    constexpr uint16_t RETRY_DELAY_MS = 400; // base delay, multiplied per attempt
}

WeatherService::WeatherService(const String& apiKey, const String& city) 
    : apiKey(apiKey), city(city) {}

String WeatherService::buildUrl() const {
    String baseUrl = "http://api.openweathermap.org/data/2.5/weather?q=";
    return baseUrl + city + "&appid=" + apiKey + "&units=imperial";
}

bool WeatherService::makeHttpRequest(const String& url, String& response, int& httpCode) {
    WiFiClient wifiClient;
    HTTPClient http;

    if (!http.begin(wifiClient, url.c_str())) {
        httpCode = 0;
        return false;
    }

    http.setTimeout(10000);
    httpCode = http.GET();
    if (httpCode == HTTP_OK_CODE) {
        response = http.getString();
        http.end();
        return true;
    }

    http.end();
    return false;
}

bool WeatherService::makeHttpRequestWithRetry(const String& url, String& response) {
    int httpCode = 0;
    for (uint8_t attempt = 1; attempt <= MAX_HTTP_ATTEMPTS; ++attempt) {
        if (makeHttpRequest(url, response, httpCode)) {
            lastError = "";
            return true;
        }

        // Build human-readable reason
        if (httpCode == 0) {
            lastError = "Weather request failed (network error)";
        } else {
            lastError = String("Weather request failed (HTTP ") + httpCode + ")";
        }

        if (attempt < MAX_HTTP_ATTEMPTS) {
            delay(RETRY_DELAY_MS * attempt);
        }
    }

    return false;
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
    JsonObject wind = root["wind"].as<JsonObject>();
    JsonObject sys  = root["sys"].as<JsonObject>();

    if (weatherArray.size() > 0) {
        weather.temperature = main["temp"] | 0.0f;
        weather.humidity = main["humidity"] | 0.0f;
        weather.pressure = main["pressure"] | 0.0f;
        weather.feelsLike = main["feels_like"] | weather.temperature;
        weather.tempMin = main["temp_min"] | weather.temperature;
        weather.tempMax = main["temp_max"] | weather.temperature;
        weather.windSpeed = wind["speed"] | 0.0f;
        weather.visibility = root["visibility"] | 0.0f; // meters
        weather.condition = weatherArray[0]["main"] | "Unknown";
        weather.description = weatherArray[0]["description"] | "No description";
        weather.sunrise = sys["sunrise"] | 0UL;
        weather.sunset  = sys["sunset"]  | 0UL;
        return true;
    }

    return false;
}

bool WeatherService::getWeather(WeatherData& weather) {
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "Weather unavailable (WiFi disconnected)";
        return false;
    }

    String url = buildUrl();
    String response;

    if (!makeHttpRequestWithRetry(url, response)) {
        // lastError already populated by helper
        return false;
    }

    if (!parseWeatherData(response, weather)) {
        lastError = "Weather response parse error";
        return false;
    }

    lastError = "";
    return true;
}