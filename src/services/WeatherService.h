// src/services/WeatherService.h

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
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
    bool makeHttpRequestWithRetry(const String& url, String& response);
    bool makeHttpRequest(const String& url, String& response, int& httpCode);
    bool parseWeatherData(const String& jsonData, WeatherData& weather);
};