// src/services/WeatherService.h

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config/Config.h"
#include "../models/WeatherData.h"

class WeatherService {
public:
    WeatherService(const String& apiKey, const String& city);
    bool getWeather(WeatherData& weather);
    const String& getLastError() const { return lastError; }

private:
    String apiKey;
    String city;
    String lastError;
    String buildUrl() const;
    String buildForecastUrl() const;
    bool makeHttpRequestWithRetry(const String& url, JsonDocument& doc);
    bool makeHttpRequest(const String& url, JsonDocument& doc, int& httpCode);
    bool parseWeatherData(JsonDocument& doc, WeatherData& weather);
    bool parseForecastData(JsonDocument& doc, WeatherData& weather);
};
