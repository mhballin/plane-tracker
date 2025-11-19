// src/models/WeatherData.h

#pragma once
#include <Arduino.h>

struct WeatherData {
    float temperature;
    float humidity;
    float pressure;
    String condition;
    String description;
    
    WeatherData() : 
        temperature(0), humidity(0), pressure(0) {}
};