// src/services/WeatherService.cpp

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
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
    String baseUrl = "https://api.openweathermap.org/data/2.5/weather?q=";
    return baseUrl + city + "&appid=" + apiKey + "&units=imperial";
}

String WeatherService::buildForecastUrl() const {
    String baseUrl = "https://api.openweathermap.org/data/2.5/forecast?q=";
    return baseUrl + city + "&appid=" + apiKey + "&units=imperial";
}

bool WeatherService::makeHttpRequest(const String& url, JsonDocument& doc, int& httpCode) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, url.c_str())) {
        httpCode = 0;
        return false;
    }

    http.setTimeout(10000);
    httpCode = http.GET();
    if (httpCode == HTTP_OK_CODE) {
        String payload = http.getString();
        http.end();
        DeserializationError err = deserializeJson(doc, payload);
        return err == DeserializationError::Ok;
    }

    http.end();
    return false;
}

bool WeatherService::makeHttpRequestWithRetry(const String& url, JsonDocument& doc) {
    int httpCode = 0;
    for (uint8_t attempt = 1; attempt <= MAX_HTTP_ATTEMPTS; ++attempt) {
        if (makeHttpRequest(url, doc, httpCode)) {
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

bool WeatherService::parseWeatherData(JsonDocument& doc, WeatherData& weather) {
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

bool WeatherService::parseForecastData(JsonDocument& doc, WeatherData& weather) {
    JsonArray list = doc["list"];
    if (list.isNull()) return false;

    weather.forecast.clear();

    // Simple aggregation: Group by day
    // Since the list is chronological, we can just track the current day
    int currentDay = -1;
    float dailyMin = 1000.0;
    float dailyMax = -1000.0;
    String dailyCondition = "";

    for (JsonObject item : list) {
        unsigned long dt = item["dt"];
        int day = (dt / 86400); // Days since epoch

        float temp_min = item["main"]["temp_min"];
        float temp_max = item["main"]["temp_max"];
        String condition = item["weather"][0]["main"] | "Clouds";

        if (currentDay == -1) {
            currentDay = day;
            dailyMin = temp_min;
            dailyMax = temp_max;
            dailyCondition = condition;
        } else if (day != currentDay) {
            // New day found, push previous day
            // Skip if it's the *current* day (we want future forecast)
            // Actually, usually we want today + next 4 days.
            // Let's just push every completed day.

            DailyForecast df;
            // Re-calculate day name based on the day index
            int dayNum = (currentDay + 4) % 7;
            const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            df.dayName = days[dayNum];

            df.tempMin = dailyMin;
            df.tempMax = dailyMax;
            df.condition = dailyCondition;

            weather.forecast.push_back(df);

            // Reset for new day
            currentDay = day;
            dailyMin = temp_min;
            dailyMax = temp_max;
            // Pick condition from noon-ish (or just first entry of day)
            // Better: pick the "worst" condition or just the one at 12:00?
            // For simplicity, keep the first one or update if it's "Clear" and we see "Rain".
            dailyCondition = condition;

            if (weather.forecast.size() >= 5) break;
        } else {
            // Same day, update min/max
            if (temp_min < dailyMin) dailyMin = temp_min;
            if (temp_max > dailyMax) dailyMax = temp_max;

            // Simple priority for condition: Rain/Snow > Clouds > Clear
            if (condition == "Rain" || condition == "Snow" || condition == "Thunderstorm") {
                dailyCondition = condition;
            }
        }
    }

    // Fix: push the last accumulated day if the loop ended without a day boundary
    if (currentDay != -1 && weather.forecast.size() < 5) {
        DailyForecast df;
        int dayNum = (currentDay + 4) % 7;
        const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        df.dayName = days[dayNum];
        df.tempMin = dailyMin;
        df.tempMax = dailyMax;
        df.condition = dailyCondition;
        weather.forecast.push_back(df);
    }

    return true;
}

bool WeatherService::getWeather(WeatherData& weather) {
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "Weather unavailable (WiFi disconnected)";
        return false;
    }

    // 1. Get Current Weather
    String url = buildUrl();
    JsonDocument doc;

    if (!makeHttpRequestWithRetry(url, doc)) {
        return false;
    }

    if (!parseWeatherData(doc, weather)) {
        lastError = "Weather response parse error";
        return false;
    }

    // 2. Get Forecast
    String forecastUrl = buildForecastUrl();
    JsonDocument forecastDoc;

    // Don't fail completely if forecast fails, just log it
    if (makeHttpRequestWithRetry(forecastUrl, forecastDoc)) {
        parseForecastData(forecastDoc, weather);
    } else {
        Serial.println("Failed to fetch forecast data");
    }

    lastError = "";
    return true;
}
