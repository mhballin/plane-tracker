// src/DisplayManager.cpp
#include "DisplayManager.h"

// Constructor
DisplayManager::DisplayManager()
    : lcd(nullptr)
    , lastDisplayUpdate(0)
    , currentBrightness(255)
    , lastCallsign("")
    , lastWeatherTemp(-999.0f)
    , lastWeatherCondition("")
    , lastWeatherHumidity(-999.0f)
    , lastWeatherPressure(-999.0f)
    , lastWeatherDescription("")
    , currentScreen(SCREEN_HOME)
    , lastScreenChange(0)
    , lastUserInteraction(0)
    , currentAircraftCount(0)
    , touchStartX(0)
    , touchStartY(0)
    , touchLastX(0)
    , touchLastY(0)
    , touchActive(false)
    , touchStartTime(0)
    , gestureInProgress(false)
    , m_statusMsg("")
    , m_statusSetAt(0)
    , m_lastUpdateTime(0)
    , m_planesViewUntil(0)
    , m_lastAcceptedTap(0)
{
}

// Destructor
DisplayManager::~DisplayManager() {
    if (lcd) {
        delete lcd;
        lcd = nullptr;
    }
}

// Initialize the display
bool DisplayManager::initialize() {
    lcd = new LGFX();
    if (!lcd) {
        Serial.println("[DisplayManager] Failed to allocate LGFX");
        return false;
    }
    
    lcd->init();
    lcd->setRotation(2); 
    lcd->setBrightness(currentBrightness);
    lcd->fillScreen(COLOR_BACKGROUND);
    
    currentScreen = SCREEN_HOME;
    lastUserInteraction = millis();
    
    Serial.println("[DisplayManager] Display initialized successfully");
    Serial.printf("[DisplayManager] Screen size: %dx%d\n", lcd->width(), lcd->height());
    
    // Test touch immediately
    delay(100);
    int testX = 0, testY = 0;
    bool touchWorks = lcd->getTouch(&testX, &testY);
    Serial.printf("[DisplayManager] Touch test: %s (raw values: %d, %d)\n", 
                  touchWorks ? "DETECTED" : "NO TOUCH", testX, testY);

    return true;
}

// Update the display with weather and aircraft data
void DisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    // Store aircraft count for display
    currentAircraftCount = aircraftCount;
    
    // Display appropriate screen based on current state
    switch (currentScreen) {
        case SCREEN_HOME:
            showWeather(weather);
            break;
            
        case SCREEN_AIRCRAFT_DETAIL:
            if (aircraft && aircraft->valid) {
                showPlane(*aircraft);
            } else {
                // No valid aircraft - show no planes screen
                currentScreen = SCREEN_NO_AIRCRAFT;
                showNoPlanes();
            }
            break;
            
        case SCREEN_NO_AIRCRAFT:
            showNoPlanes();
            break;
    }

    // Always refresh the status bar (clock / status message)
    // even if the main screen content didn't change, so the
    // time and transient status messages stay current.
    drawStatusBar();
}

// Set display brightness
void DisplayManager::setBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    if (lcd) {
        lcd->setBrightness(brightness);
    }
}

// Adjust brightness by delta amount
void DisplayManager::adjustBrightness(int delta) {
    int newBrightness = currentBrightness + delta;
    newBrightness = constrain(newBrightness, Config::BRIGHTNESS_MIN, Config::BRIGHTNESS_MAX);
    
    if (newBrightness != currentBrightness) {
        setBrightness(newBrightness);
        Serial.printf("[DisplayManager] Brightness adjusted to %d\\n", currentBrightness);
        setStatusMessage("Brightness: " + String(currentBrightness));
    }
}

// Clear the display
void DisplayManager::clear() {
    if (lcd) {
        lcd->fillScreen(COLOR_BACKGROUND);
    }
}

// Show error message
void DisplayManager::showError(const char* title, const char* message) {
    if (!lcd) return;
    
    lcd->fillScreen(COLOR_BACKGROUND);
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    
    // Center title
    int16_t titleX = (lcd->width() - strlen(title) * 12) / 2;
    lcd->setCursor(titleX, lcd->height() / 2 - 40);
    lcd->print(title);
    
    // Center message
    lcd->setTextSize(1);
    int16_t msgX = (lcd->width() - strlen(message) * 6) / 2;
    lcd->setCursor(msgX, lcd->height() / 2 + 20);
    lcd->print(message);
}

// ========================================
// Screen State Management
// ========================================
void DisplayManager::setScreen(ScreenState screen) {
    if (currentScreen != screen) {
        currentScreen = screen;
        lastScreenChange = millis();
        lastUserInteraction = millis();
        Serial.printf("[DisplayManager] Screen changed to: %d\\n", screen);
    }
}

bool DisplayManager::shouldReturnToHome() const {
    // Only auto-return if we're not already on home and enough time has passed
    if (currentScreen == SCREEN_HOME) {
        return false;
    }
    
    unsigned long elapsed = millis() - lastUserInteraction;
    return elapsed >= Config::UI_AUTO_HOME_MS;
}

void DisplayManager::resetIdleTimer() {
    lastUserInteraction = millis();
}

// Set status message
void DisplayManager::setStatusMessage(const String& msg) {
    m_statusMsg = msg;
    m_statusSetAt = millis();
    
    if (Config::DEBUG_SERIAL) {
        Serial.printf("[DisplayManager] Status: %s\\n", msg.c_str());
    }
}

// Set last update timestamp
void DisplayManager::setLastUpdateTimestamp(time_t epochSecs) {
    m_lastUpdateTime = epochSecs;
}

// Reset cached data to force redraw
void DisplayManager::resetCachedData() {
    lastCallsign = "";
    lastWeatherTemp = -999.0f;
    lastWeatherCondition = "";
    lastWeatherHumidity = -999.0f;
    lastWeatherPressure = -999.0f;
    lastWeatherDescription = "";
}

// Draw header
void DisplayManager::drawHeader(const char* title) {
    if (!lcd) return;
    
    lcd->fillRect(0, 0, lcd->width(), 40, COLOR_PANEL);
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(10, 10);
    lcd->print(title);
}

// Draw card
void DisplayManager::drawCard(int x, int y, int w, int h, const char* title) {
    if (!lcd) return;
    
    lcd->fillRect(x, y, w, h, COLOR_PANEL);
    lcd->drawRect(x, y, w, h, COLOR_ACCENT);
    
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(x + 10, y + 10);
    lcd->print(title);
}

// Draw divider line
void DisplayManager::drawDivider(int y) {
    if (!lcd) return;
    lcd->drawLine(MARGIN_MEDIUM, y, lcd->width() - MARGIN_MEDIUM, y, COLOR_DIVIDER);
}

// Draw status bar
void DisplayManager::drawStatusBar() {
    if (!lcd) return;
    
    // Status bar at bottom
    int barY = lcd->height() - STATUS_BAR_HEIGHT;
    lcd->fillRect(0, barY, lcd->width(), STATUS_BAR_HEIGHT, COLOR_PANEL);
    lcd->drawLine(0, barY, lcd->width(), barY, COLOR_DIVIDER);
    
    lcd->setTextColor(COLOR_TEXT_DIM);
    lcd->setTextSize(1);
    
    // Left side: Status message or time
    if (m_statusMsg.length() > 0 && (millis() - m_statusSetAt < Config::UI_STATUS_MS)) {
        lcd->setCursor(MARGIN_MEDIUM, barY + 14);
        lcd->print(m_statusMsg);
    } else {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            lcd->setCursor(MARGIN_MEDIUM, barY + 14);
            lcd->print(timeStr);
        }
    }
    
    // Right side: Brightness indicator
    lcd->setCursor(lcd->width() - 90, barY + 14);
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->print("Bright:");
    lcd->setTextColor(COLOR_TEXT_DIM);
    lcd->printf(" %d%%", (currentBrightness * 100) / 255);
}

// Draw metric card
// Draw metric card
void DisplayManager::drawMetricCard(int x, int y, int w, int h, 
                                     const char* label, 
                                     const String& primaryValue, 
                                     const String& secondaryValue) {
    if (!lcd) return;
    
    lcd->fillRect(x, y, w, h, COLOR_PANEL);
    lcd->drawRect(x, y, w, h, COLOR_ACCENT);
    
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(x + 10, y + 10);
    lcd->print(label);
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(x + 10, y + 30);
    lcd->print(primaryValue);
    
    if (secondaryValue.length() > 0) {
        lcd->setTextSize(1);
        lcd->setCursor(x + 10, y + 55);
        lcd->print(secondaryValue);
    }
}

// Draw a single forecast day
void DisplayManager::drawForecastDay(int x, int y, const char* day, const char* icon, const char* low, const char* high) {
    if (!lcd) return;
    
    // Day name
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(x, y);
    lcd->print(day);
    
    // Icon placeholder (small)
    drawWeatherIcon(x + 80, y + 8, 24, String(icon));
    
    // Temperature range with visual bar
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(x + 130, y);
    lcd->print(low);
    
    // Draw temperature range bar
    int barX = x + 170;
    int barY = y + 5;
    int barW = 100;
    int barH = 8;
    lcd->fillRect(barX, barY, barW, barH, COLOR_PANEL);
    lcd->fillRect(barX + 10, barY + 1, barW - 40, barH - 2, COLOR_ACCENT);
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setCursor(x + 280, y);
    lcd->print(high);
}

// Draw plane counter badge
void DisplayManager::drawPlaneCounter(int x, int y, int count) {
    if (!lcd) return;
    
    // Draw plane icon
    drawPlaneIcon(x, y, 30);
    
    // Draw count badge
    int badgeX = x + 20;
    int badgeY = y - 10;
    int badgeRadius = 12;
    
    lcd->fillCircle(badgeX, badgeY, badgeRadius, COLOR_ACCENT);
    lcd->drawCircle(badgeX, badgeY, badgeRadius, COLOR_TEXT);
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(1);
    char countStr[4];
    snprintf(countStr, sizeof(countStr), "%d", count);
    int textX = badgeX - (strlen(countStr) * 3);
    lcd->setCursor(textX, badgeY - 4);
    lcd->print(countStr);
}

// Weather icon drawing methods (simplified placeholders)
void DisplayManager::drawSunIcon(int x, int y, int size) {
    if (!lcd) return;
    lcd->fillCircle(x, y, size / 3, COLOR_ACCENT);
}

void DisplayManager::drawCloudIcon(int x, int y, int size) {
    if (!lcd) return;
    lcd->fillCircle(x, y, size / 3, COLOR_SUBTEXT);
    lcd->fillCircle(x + size / 4, y, size / 4, COLOR_SUBTEXT);
}

void DisplayManager::drawRainIcon(int x, int y, int size) {
    if (!lcd) return;
    drawCloudIcon(x, y, size);
    lcd->drawLine(x - size / 6, y + size / 3, x - size / 6, y + size / 2, TFT_BLUE);
    lcd->drawLine(x, y + size / 3, x, y + size / 2, TFT_BLUE);
    lcd->drawLine(x + size / 6, y + size / 3, x + size / 6, y + size / 2, TFT_BLUE);
}

void DisplayManager::drawPartlyCloudyIcon(int x, int y, int size) {
    if (!lcd) return;
    drawSunIcon(x - size / 4, y - size / 4, size / 2);
    drawCloudIcon(x + size / 4, y + size / 4, size / 2);
}

void DisplayManager::drawPlaneIcon(int x, int y, int size) {
    if (!lcd) return;
    // Simple plane shape
    lcd->fillTriangle(x, y - size / 2, x - size / 3, y + size / 3, x + size / 3, y + size / 3, COLOR_ACCENT);
    lcd->fillRect(x - size / 6, y - size / 4, size / 3, size / 2, COLOR_ACCENT);
}

// Select appropriate weather icon based on condition
void DisplayManager::drawWeatherIcon(int x, int y, int size, const String& condition) {
    String cond = condition;
    cond.toLowerCase();
    
    if (cond.indexOf("clear") >= 0 || cond.indexOf("sun") >= 0) {
        drawSunIcon(x, y, size);
    } else if (cond.indexOf("cloud") >= 0 && cond.indexOf("part") >= 0) {
        drawPartlyCloudyIcon(x, y, size);
    } else if (cond.indexOf("rain") >= 0 || cond.indexOf("drizzle") >= 0) {
        drawRainIcon(x, y, size);
    } else if (cond.indexOf("cloud") >= 0) {
        drawCloudIcon(x, y, size);
    } else {
        // Default to cloud
        drawCloudIcon(x, y, size);
    }
}

// Show clock screen
void DisplayManager::showClock() {
    if (!lcd) return;
    
    lcd->fillScreen(COLOR_BACKGROUND);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[16];
        char dateStr[32];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", 
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        
        lcd->setTextColor(COLOR_TEXT);
        lcd->setTextSize(4);
        int16_t timeX = (lcd->width() - strlen(timeStr) * 24) / 2;
        lcd->setCursor(timeX, lcd->height() / 2 - 40);
        lcd->print(timeStr);
        
        lcd->setTextSize(2);
        int16_t dateX = (lcd->width() - strlen(dateStr) * 12) / 2;
        lcd->setCursor(dateX, lcd->height() / 2 + 20);
        lcd->print(dateStr);
    }
    
    drawStatusBar();
}

// Show weather screen (HOME)
void DisplayManager::showWeather(const WeatherData& weather) {
    if (!lcd) return;
    
    // Only redraw if data changed to reduce flicker
    if (weather.temperature == lastWeatherTemp && 
        weather.condition == lastWeatherCondition) {
        return;
    }
    
    lcd->fillScreen(COLOR_BACKGROUND);
    
    // Header: "WEATHER STATION" + Location
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(20, 10);
    lcd->print("WEATHER STATION");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(20, 30);
    lcd->print("Portland, ME");
    
    // Date and time in top right
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char dateStr[32];
        char timeStr[16];
        const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(dateStr, sizeof(dateStr), "%s, Oct %d", dayNames[timeinfo.tm_wday], timeinfo.tm_mday);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        
        lcd->setTextColor(COLOR_SUBTEXT);
        lcd->setTextSize(1);
        int dateX = lcd->width() - strlen(dateStr) * 6 - 20;
        lcd->setCursor(dateX, 10);
        lcd->print(dateStr);
        
        lcd->setTextColor(COLOR_TEXT);
        lcd->setTextSize(2);
        int timeX = lcd->width() - strlen(timeStr) * 12 - 20;
        lcd->setCursor(timeX, 30);
        lcd->print(timeStr);
    }
    
    // Current weather - Large temp and icon
    int currentWeatherY = 80;
    
    // Big weather icon on left
    drawWeatherIcon(60, currentWeatherY + 40, 80, weather.condition);
    
    // Temperature
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.0f°", weather.temperature);
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(6);
    lcd->setCursor(150, currentWeatherY);
    lcd->print(tempStr);
    
    // Condition description
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(2);
    lcd->setCursor(150, currentWeatherY + 75);
    lcd->print(weather.description);
    
    // High/Low/Feels Like in small cards
    int detailY = currentWeatherY + 110;
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    
    lcd->setCursor(150, detailY);
    lcd->print("HIGH");
    lcd->setCursor(220, detailY);
    lcd->print("LOW");
    lcd->setCursor(290, detailY);
    lcd->print("FEELS LIKE");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    char highStr[8], lowStr[8], feelsStr[8];
    snprintf(highStr, sizeof(highStr), "72°");
    snprintf(lowStr, sizeof(lowStr), "58°");
    snprintf(feelsStr, sizeof(feelsStr), "65°");
    
    lcd->setCursor(150, detailY + 15);
    lcd->print(highStr);
    lcd->setCursor(220, detailY + 15);
    lcd->print(lowStr);
    lcd->setCursor(290, detailY + 15);
    lcd->print(feelsStr);
    
    // Humidity, Wind, Visibility, Pressure in bottom left
    int metricsY = 260;
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    
    lcd->setCursor(20, metricsY);
    lcd->print("HUMIDITY");
    lcd->setCursor(120, metricsY);
    lcd->print("WIND");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    char humidStr[16], windStr[16];
    snprintf(humidStr, sizeof(humidStr), "%.0f%%", weather.humidity);
    snprintf(windStr, sizeof(windStr), "12 mph");
    
    lcd->setCursor(20, metricsY + 15);
    lcd->print(humidStr);
    lcd->setCursor(120, metricsY + 15);
    lcd->print(windStr);
    
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(20, metricsY + 50);
    lcd->print("VISIBILITY");
    lcd->setCursor(120, metricsY + 50);
    lcd->print("PRESSURE");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    char visStr[16], pressStr[16];
    snprintf(visStr, sizeof(visStr), "10 mi");
    snprintf(pressStr, sizeof(pressStr), "%.0fmb", weather.pressure);
    
    lcd->setCursor(20, metricsY + 65);
    lcd->print(visStr);
    lcd->setCursor(120, metricsY + 65);
    lcd->print(pressStr);
    
    // 7-DAY FORECAST on the right side
    int forecastX = 440;
    int forecastY = 80;
    
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(forecastX, forecastY);
    lcd->print("7-DAY FORECAST");
    
    // Forecast days (placeholder data matching your Figma)
    const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const char* icons[] = {"cloud", "rain", "rain", "cloud", "sun", "sun", "sun"};
    const char* lows[] = {"58°", "56°", "52°", "54°", "60°", "62°", "58°"};
    const char* highs[] = {"72°", "70°", "65°", "68°", "75°", "78°", "74°"};
    
    for (int i = 0; i < 7; i++) {
        int dayY = forecastY + 30 + (i * 45);
        drawForecastDay(forecastX, dayY, days[i], icons[i], lows[i], highs[i]);
    }
    
    lastWeatherTemp = weather.temperature;
    lastWeatherCondition = weather.condition;
}

// Show aircraft detail screen
void DisplayManager::showPlane(const Aircraft& aircraft) {
    if (!lcd) return;
    
    // Only redraw if aircraft changed
    if (aircraft.callsign == lastCallsign) {
        return;
    }
    
    lcd->fillScreen(COLOR_BACKGROUND);
    
    // Header with plane icon
    drawPlaneIcon(30, 30, 24);
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(70, 23);
    lcd->print(aircraft.callsign.c_str());
    
    // Aircraft type
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(70, 45);
    lcd->print("Boeing 737-800");  // Placeholder - will use aircraft.aircraftType later
    
    // Route (FROM -> TO) in top right
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(500, 20);
    lcd->print("FROM");
    lcd->setCursor(650, 20);
    lcd->print("TO");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(2);
    lcd->setCursor(500, 35);
    lcd->print("Boston, MA");  // Placeholder - will use aircraft.origin later
    
    // Arrow
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setCursor(630, 38);
    lcd->print("->");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setCursor(650, 35);
    lcd->print("Chicago, IL");  // Placeholder - will use aircraft.destination later
    
    // Main metrics in large cards (2x2 grid)
    int cardY = 100;
    int cardW = 180;
    int cardH = 100;
    int spacingX = 20;
    int spacingY = 20;
    
    // Row 1: Altitude and Speed
    char altStr[16], speedStr[16];
    snprintf(altStr, sizeof(altStr), "%.0f", aircraft.altitude * 3.28084f);  // m to ft
    snprintf(speedStr, sizeof(speedStr), "%.0f", aircraft.velocity * 1.94384f);  // m/s to kts
    
    drawMetricCard(spacingX, cardY, cardW, cardH, "ALTITUDE", String(altStr), "ft");
    drawMetricCard(spacingX + cardW + spacingX, cardY, cardW, cardH, "SPEED", String(speedStr), "kts");
    
    // Row 2: Heading and Bearing
    char headStr[16], bearStr[16];
    snprintf(headStr, sizeof(headStr), "%.0f°", aircraft.heading);
    snprintf(bearStr, sizeof(bearStr), "315°");  // Placeholder - calculate bearing from home
    
    drawMetricCard(spacingX, cardY + cardH + spacingY, cardW, cardH, "HEADING", String(headStr), "");
    drawMetricCard(spacingX + cardW + spacingX, cardY + cardH + spacingY, cardW, cardH, "BEARING", String(bearStr), "");
    
    // Distance metric (larger, on right side)
    int distCardX = 440;
    int distCardW = 340;
    int distCardH = 220;
    
    // Calculate distance from home (placeholder calculation)
    float latDiff = aircraft.latitude - Config::HOME_LAT;
    float lonDiff = aircraft.longitude - Config::HOME_LON;
    float distance = sqrt(latDiff * latDiff + lonDiff * lonDiff) * 111.0f;  // Rough km
    
    char distStr[16];
    snprintf(distStr, sizeof(distStr), "%.1f", distance * 0.621371f);  // km to mi
    
    lcd->fillRect(distCardX, cardY, distCardW, distCardH, COLOR_PANEL);
    lcd->drawRect(distCardX, cardY, distCardW, distCardH, COLOR_ACCENT);
    
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(1);
    lcd->setCursor(distCardX + 20, cardY + 20);
    lcd->print("DISTANCE");
    
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(6);
    lcd->setCursor(distCardX + 20, cardY + 80);
    lcd->print(distStr);
    
    lcd->setTextSize(2);
    lcd->setCursor(distCardX + 20, cardY + 150);
    lcd->print("nmi");
    
    // Tracking status
    lcd->fillCircle(spacingX + 10, 350, 5, TFT_GREEN);
    lcd->setTextColor(COLOR_TEXT);
    lcd->setTextSize(1);
    lcd->setCursor(spacingX + 25, 345);
    lcd->print("TRACKING ACTIVE");
    
    lastCallsign = aircraft.callsign;
}

// Show "no planes" message
void DisplayManager::showNoPlanes() {
    if (!lcd) return;
    
    lcd->fillScreen(COLOR_BACKGROUND);
    
    // Draw a large plane icon (grayed out)
    int centerX = lcd->width() / 2;
    int centerY = lcd->height() / 2 - 40;
    
    lcd->setTextColor(COLOR_PANEL);
    drawPlaneIcon(centerX, centerY, 80);
    
    // Message
    lcd->setTextColor(COLOR_SUBTEXT);
    lcd->setTextSize(3);
    const char* msg = "No aircraft detected";
    int16_t msgX = (lcd->width() - strlen(msg) * 18) / 2;
    lcd->setCursor(msgX, centerY + 80);
    lcd->print(msg);
    
    // Hint
    lcd->setTextSize(1);
    const char* hint = "Swipe right to return home";
    int16_t hintX = (lcd->width() - strlen(hint) * 6) / 2;
    lcd->setCursor(hintX, centerY + 140);
    lcd->print(hint);
}

// Show radar view (placeholder)
void DisplayManager::showRadar(const Aircraft& aircraft) {
    // Placeholder for radar view implementation
    showPlane(aircraft);
}

// ========================================
// Gesture Detection System
// ========================================
void DisplayManager::processTouch() {
    if (!lcd) return;
    
    int touchX = 0, touchY = 0;
    bool haveTouch = lcd->getTouch(&touchX, &touchY);

    if (haveTouch && touchX > 0 && touchY > 0) {
        touchLastX = touchX;
        touchLastY = touchY;

        if (!touchActive) {
            // Touch started
            touchActive = true;
            touchStartX = touchLastX;
            touchStartY = touchLastY;
            touchStartTime = millis();
            gestureInProgress = false;
            
            Serial.printf("[Touch] Touch started at (%d, %d)\n", touchStartX, touchStartY);
        } else {
            // still active - update last user interaction
            lastUserInteraction = millis();
        }
    } else {
        // Touch ended (or no reliable samples)
        if (touchActive && !gestureInProgress) {
            touchActive = false;
            
            Serial.printf("[Touch] Touch ended at (%d, %d)\n", touchLastX, touchLastY);
            
            checkForGestures();
        } else {
            touchActive = false;
            gestureInProgress = false;
        }
    }
}

void DisplayManager::checkForGestures() {
    int deltaX = 0;
    int deltaY = 0;
    
    if (detectSwipe(deltaX, deltaY)) {
        unsigned long duration = millis() - touchStartTime;
        
        // Prevent double-gestures
        static unsigned long lastGestureTime = 0;
        if (millis() - lastGestureTime < Config::GESTURE_DEBOUNCE_MS) {
            return;
        }
        lastGestureTime = millis();
        
        // Determine primary direction
        if (abs(deltaX) > abs(deltaY)) {
            // Horizontal swipe
            if (deltaX > 0) {
                handleSwipeRight();
            } else {
                handleSwipeLeft();
            }
        } else {
            // Vertical swipe
            if (deltaY > 0) {
                handleSwipeDown();
            } else {
                handleSwipeUp();
            }
        }
        
        gestureInProgress = true;
        resetIdleTimer();
    }
}

bool DisplayManager::detectSwipe(int& deltaX, int& deltaY) {
    deltaX = touchLastX - touchStartX;
    deltaY = touchLastY - touchStartY;
    
    unsigned long duration = millis() - touchStartTime;
    
    // Check if movement is significant enough
    int absX = abs(deltaX);
    int absY = abs(deltaY);

    // Primary axis must be larger than orthogonal by a factor to avoid diagonals
    const float AXIS_DOMINANCE = 1.5f; // primary axis must be 1.5x the other

    if (absX < Config::GESTURE_SWIPE_THRESHOLD && absY < Config::GESTURE_SWIPE_THRESHOLD) {
        return false; // not enough movement on either axis
    }

    if (duration > Config::GESTURE_MAX_TIME_MS) {
        return false; // took too long
    }

    // Compute per-axis velocity (pixels/sec)
    float vx = (absX * 1000.0f) / (duration + 1);
    float vy = (absY * 1000.0f) / (duration + 1);

    // Check axis dominance and velocity on primary axis
    if (absX >= absY * AXIS_DOMINANCE) {
        // horizontal primary
        if (vx < Config::GESTURE_SWIPE_VELOCITY_MIN) return false;
        return true;
    } else if (absY >= absX * AXIS_DOMINANCE) {
        // vertical primary
        if (vy < Config::GESTURE_SWIPE_VELOCITY_MIN) return false;
        return true;
    }

    return false; // diagonal or ambiguous
}

void DisplayManager::handleSwipeLeft() {
    Serial.println("[Gesture] Swipe LEFT");
    
    if (currentScreen == SCREEN_HOME) {
        // From home, go to aircraft view
        setScreen(SCREEN_AIRCRAFT_DETAIL);
        setStatusMessage("Aircraft View");
    }
    // If already in aircraft view, do nothing (main.cpp handles aircraft cycling)
}

void DisplayManager::handleSwipeRight() {
    Serial.println("[Gesture] Swipe RIGHT");
    
    if (currentScreen == SCREEN_AIRCRAFT_DETAIL || currentScreen == SCREEN_NO_AIRCRAFT) {
        // From aircraft view, return to home
        setScreen(SCREEN_HOME);
        setStatusMessage("Home");
    }
    // If already on home, do nothing
}

void DisplayManager::handleSwipeUp() {
    Serial.println("[Gesture] Swipe UP - Increase brightness");
    adjustBrightness(Config::BRIGHTNESS_STEP);
}

void DisplayManager::handleSwipeDown() {
    Serial.println("[Gesture] Swipe DOWN - Decrease brightness");
    adjustBrightness(-Config::BRIGHTNESS_STEP);
}

// Set backlight brightness with fade
void DisplayManager::setBacklight(uint8_t brightness) {
    setBrightness(brightness);
}

// Fade transition effect
void DisplayManager::fadeTransition(bool fadeIn) {
    if (!lcd) return;
    
    int steps = 20;
    int delayMs = 10;
    
    if (fadeIn) {
        for (int i = 0; i <= steps; i++) {
            lcd->setBrightness((currentBrightness * i) / steps);
            delay(delayMs);
        }
    } else {
        for (int i = steps; i >= 0; i--) {
            lcd->setBrightness((currentBrightness * i) / steps);
            delay(delayMs);
        }
    }
}

// Format time string
String DisplayManager::formatTime(const struct tm& timeinfo) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buffer);
}

// Format date string
String DisplayManager::formatDate(const struct tm& timeinfo) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    return String(buffer);
}
