// src/models/WeatherData.h

#pragma once
#include <Arduino.h>

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
    
    WeatherData() : 
        temperature(0), humidity(0), pressure(0),
        feelsLike(0), tempMin(0), tempMax(0),
        windSpeed(0), visibility(0) {}
};