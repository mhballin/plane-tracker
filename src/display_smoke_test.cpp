// Touch Diagnostics Harness - compiled when SMOKE_TEST is defined
// Visualizes touch input, detects gestures, and reports detailed metrics
// Build: pio run -e smoke-test -t upload && pio device monitor -b 115200
#ifdef SMOKE_TEST
#include <Arduino.h>
#include "DisplayManager.h"
#include "config/Config.h"

DisplayManager* dm;
LGFX* lcd;

// Touch trail visualization
#define MAX_TRAIL_POINTS 100
struct TrailPoint {
    int x, y;
    unsigned long timestamp;
};
TrailPoint trail[MAX_TRAIL_POINTS];
int trailIndex = 0;
int trailCount = 0;

// Touch state tracking
bool touchActive = false;
int touchStartX = 0, touchStartY = 0;
int touchLastX = 0, touchLastY = 0;
unsigned long touchStartTime = 0;
int gestureCount = 0;

void drawUI() {
    // Header
    lcd->fillRect(0, 0, 800, 50, TFT_NAVY);
    lcd->setTextColor(TFT_WHITE);
    lcd->setTextSize(2);
    lcd->setCursor(10, 15);
    lcd->print("TOUCH DIAGNOSTICS HARNESS");
    
    // Instructions
    lcd->fillRect(0, 430, 800, 50, TFT_DARKGREY);
    lcd->setTextSize(1);
    lcd->setTextColor(TFT_YELLOW);
    lcd->setCursor(10, 435);
    lcd->print("Swipe to test gestures | Touch coordinates and metrics displayed in serial monitor");
    lcd->setCursor(10, 450);
    lcd->printf("Gestures detected: %d | Current thresholds: dist=%dpx vel=%dpx/s", 
                gestureCount, Config::GESTURE_SWIPE_THRESHOLD, Config::GESTURE_SWIPE_VELOCITY_MIN);
}

void clearTrail() {
    trailCount = 0;
    trailIndex = 0;
}

void addTrailPoint(int x, int y) {
    trail[trailIndex].x = x;
    trail[trailIndex].y = y;
    trail[trailIndex].timestamp = millis();
    trailIndex = (trailIndex + 1) % MAX_TRAIL_POINTS;
    if (trailCount < MAX_TRAIL_POINTS) trailCount++;
}

void drawTrail() {
    // Draw fade-out trail
    for (int i = 0; i < trailCount; i++) {
        int idx = (trailIndex - trailCount + i + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        unsigned long age = millis() - trail[idx].timestamp;
        
        if (age > 2000) continue; // Skip old points
        
        // Fade color based on age (0=bright, 2000ms=dim)
        uint8_t brightness = 255 - (age * 255 / 2000);
        uint16_t color = lcd->color565(brightness, brightness / 2, brightness);
        
        lcd->fillCircle(trail[idx].x, trail[idx].y, 3, color);
    }
}

void analyzeGesture(int startX, int startY, int endX, int endY, unsigned long duration) {
    int deltaX = endX - startX;
    int deltaY = endY - startY;
    int absX = abs(deltaX);
    int absY = abs(deltaY);
    
    float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
    float velocityX = (absX * 1000.0f) / (duration + 1);
    float velocityY = (absY * 1000.0f) / (duration + 1);
    float velocity = (distance * 1000.0f) / (duration + 1);
    
    Serial.println("\n========== GESTURE ANALYSIS ==========");
    Serial.printf("Start: (%d, %d) → End: (%d, %d)\n", startX, startY, endX, endY);
    Serial.printf("Delta: X=%d Y=%d\n", deltaX, deltaY);
    Serial.printf("Distance: %.1f px\n", distance);
    Serial.printf("Duration: %lu ms\n", duration);
    Serial.printf("Velocity: %.1f px/s (X: %.1f, Y: %.1f)\n", velocity, velocityX, velocityY);
    
    // Determine dominant axis
    const float AXIS_DOMINANCE = 1.5f;
    String direction = "DIAGONAL/AMBIGUOUS";
    bool isSwipe = false;
    
    if (absX >= absY * AXIS_DOMINANCE) {
        direction = deltaX > 0 ? "RIGHT (horizontal)" : "LEFT (horizontal)";
        if (velocityX >= Config::GESTURE_SWIPE_VELOCITY_MIN && 
            absX >= Config::GESTURE_SWIPE_THRESHOLD &&
            duration <= Config::GESTURE_MAX_TIME_MS) {
            isSwipe = true;
        }
    } else if (absY >= absX * AXIS_DOMINANCE) {
        direction = deltaY > 0 ? "DOWN (vertical)" : "UP (vertical)";
        if (velocityY >= Config::GESTURE_SWIPE_VELOCITY_MIN && 
            absY >= Config::GESTURE_SWIPE_THRESHOLD &&
            duration <= Config::GESTURE_MAX_TIME_MS) {
            isSwipe = true;
        }
    }
    
    Serial.printf("Direction: %s\n", direction.c_str());
    Serial.printf("Axis dominance: X/Y ratio = %.2f\n", (float)absX / (absY + 1));
    
    // Check against thresholds
    Serial.println("\n--- Threshold Checks ---");
    Serial.printf("Distance threshold (%d px): %s (%s by %.1f px)\n", 
                  Config::GESTURE_SWIPE_THRESHOLD,
                  distance >= Config::GESTURE_SWIPE_THRESHOLD ? "PASS" : "FAIL",
                  distance >= Config::GESTURE_SWIPE_THRESHOLD ? "exceeded" : "short",
                  abs(distance - Config::GESTURE_SWIPE_THRESHOLD));
    Serial.printf("Velocity threshold (%d px/s): %s (%.1f px/s)\n",
                  Config::GESTURE_SWIPE_VELOCITY_MIN,
                  velocity >= Config::GESTURE_SWIPE_VELOCITY_MIN ? "PASS" : "FAIL",
                  velocity);
    Serial.printf("Duration threshold (%d ms): %s (%lu ms)\n",
                  Config::GESTURE_MAX_TIME_MS,
                  duration <= Config::GESTURE_MAX_TIME_MS ? "PASS" : "FAIL",
                  duration);
    Serial.printf("Axis dominance (1.5x): %s\n",
                  direction.indexOf("DIAGONAL") < 0 ? "PASS" : "FAIL");
    
    Serial.printf("\n>>> RESULT: %s <<<\n", isSwipe ? "SWIPE DETECTED" : "NOT A SWIPE");
    if (isSwipe) {
        gestureCount++;
        Serial.printf("Total gestures detected: %d\n", gestureCount);
    }
    Serial.println("======================================\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== TOUCH DIAGNOSTICS HARNESS ===");
    Serial.println("[BOOT] Initializing display and touch...\n");
    
    dm = new DisplayManager();
    if (!dm || !dm->initialize()) {
        Serial.println("[ERROR] Display initialization failed");
        while (1) delay(1000);
    }
    
    lcd = dm->getDisplay();
    if (!lcd) {
        Serial.println("[ERROR] Could not get LCD instance");
        while (1) delay(1000);
    }
    
    Serial.println("[INIT] ✅ Display and touch ready");
    Serial.printf("[INIT] Screen size: %dx%d\n", lcd->width(), lcd->height());
    Serial.printf("[CONFIG] Swipe threshold: %d px\n", Config::GESTURE_SWIPE_THRESHOLD);
    Serial.printf("[CONFIG] Velocity min: %d px/s\n", Config::GESTURE_SWIPE_VELOCITY_MIN);
    Serial.printf("[CONFIG] Max duration: %d ms\n", Config::GESTURE_MAX_TIME_MS);
    Serial.printf("[CONFIG] Debounce: %d ms\n\n", Config::GESTURE_DEBOUNCE_MS);
    
    // Initial UI
    lcd->fillScreen(TFT_BLACK);
    drawUI();
    
    Serial.println("=== READY FOR TOUCH INPUT ===");
    Serial.println("Touch the screen to see coordinates and gesture analysis\n");
}

void loop() {
    int touchX = 0, touchY = 0;
    bool haveTouch = lcd->getTouch(&touchX, &touchY);
    
    if (haveTouch && touchX > 0 && touchY > 0) {
        // Serial output for raw coordinates
        Serial.printf("RAW: %d, %d\n", touchX, touchY);
        
        if (!touchActive) {
            // Touch started
            touchActive = true;
            touchStartX = touchX;
            touchStartY = touchY;
            touchStartTime = millis();
            clearTrail();
            
            Serial.printf("\n[TOUCH START] (%d, %d) at %lu ms\n", touchX, touchY, touchStartTime);
            
            // Clear work area for new gesture
            lcd->fillRect(0, 50, 800, 380, TFT_BLACK);
            
            // Draw crosshair at start
            lcd->drawLine(touchX - 10, touchY, touchX + 10, touchY, TFT_GREEN);
            lcd->drawLine(touchX, touchY - 10, touchX, touchY + 10, TFT_GREEN);
        }
        
        touchLastX = touchX;
        touchLastY = touchY;
        addTrailPoint(touchX, touchY);
        
        // Redraw work area
        lcd->fillRect(0, 50, 800, 380, TFT_BLACK);
        drawTrail();
        
        // Draw current position
        lcd->fillCircle(touchX, touchY, 5, TFT_RED);
        
        // Draw line from start to current
        lcd->drawLine(touchStartX, touchStartY, touchX, touchY, TFT_CYAN);
        
        // Show live metrics
        int deltaX = touchX - touchStartX;
        int deltaY = touchY - touchStartY;
        float dist = sqrt(deltaX * deltaX + deltaY * deltaY);
        unsigned long dur = millis() - touchStartTime;
        
        lcd->setTextSize(1);
        lcd->setTextColor(TFT_WHITE);
        lcd->setCursor(10, 60);
        lcd->printf("Live: dist=%.0fpx dx=%d dy=%d dur=%lums", dist, deltaX, deltaY, dur);
        
    } else if (touchActive) {
        // Touch ended
        touchActive = false;
        unsigned long duration = millis() - touchStartTime;
        
        Serial.printf("[TOUCH END] (%d, %d) at %lu ms (duration: %lu ms)\n", 
                     touchLastX, touchLastY, millis(), duration);
        
        // Analyze the gesture
        analyzeGesture(touchStartX, touchStartY, touchLastX, touchLastY, duration);
        
        // Draw end crosshair
        lcd->drawLine(touchLastX - 10, touchLastY, touchLastX + 10, touchLastY, TFT_RED);
        lcd->drawLine(touchLastX, touchLastY - 10, touchLastX, touchLastY + 10, TFT_RED);
        
        // Update gesture counter in UI
        drawUI();
    }
    
    delay(10); // Small delay for stability
}
#endif // SMOKE_TEST
