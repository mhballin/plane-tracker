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

private:
    String apiKey;
    String city;
    String buildUrl() const;
    String makeHttpRequest(const String& url);
    bool parseWeatherData(const String& jsonData, WeatherData& weather);
};