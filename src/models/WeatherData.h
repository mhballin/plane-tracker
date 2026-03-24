// src/models/WeatherData.h

#pragma once
#include <Arduino.h>

#include <vector>

struct DailyForecast {
    String dayName;   // e.g., "Mon"
    float tempMin;
    float tempMax;
    String condition; // e.g., "Clouds"
};

struct WeatherData {
    float temperature;
    float humidity;
    float pressure;
    float feelsLike;
    float tempMin;
    float tempMax;
    float windSpeed;
    float visibility;
    String condition;
    String description;
    unsigned long sunrise; // epoch seconds
    unsigned long sunset;  // epoch seconds
    
    std::vector<DailyForecast> forecast;
    
    WeatherData() : 
        temperature(0), humidity(0), pressure(0),
        feelsLike(0), tempMin(0), tempMax(0),
        windSpeed(0), visibility(0), sunrise(0), sunset(0) {}
};