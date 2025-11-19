// This smoke-test file is compiled only when SMOKE_TEST is defined.
// To enable: add `-DSMOKE_TEST` to build flags or define in PlatformIO build env.
#ifdef SMOKE_TEST
#include <Arduino.h>
#include "DisplayManager.h"
#include <esp_heap_caps.h>

DisplayManager* dm;

void setup() {
  Serial.begin(115200);
  // Give the serial monitor a bit more time and print heartbeats so we can
  // observe early boot logs before attempting any display allocation.
  for (int i = 0; i < 5; ++i) {
    delay(500);
    Serial.print("boot stage "); Serial.println(i + 1);
  }
  Serial.println("Display smoke test starting... (delayed)");

  // Wait a moment so you can attach the serial monitor
  delay(2000);

  // --- PSRAM / DMA quick health check ---
  Serial.println("=== PSRAM test ===");
  size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
  size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  Serial.printf("free_spiram=%u free_dma=%u free_internal=%u\n", (unsigned)free_spiram, (unsigned)free_dma, (unsigned)free_internal);

  size_t try_size = 1024 * 1024; // 1MB
  void* p = heap_caps_malloc(try_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  Serial.printf("alloc %u bytes from SPIRAM -> %p\n", (unsigned)try_size, p);
  if (p) {
    memset(p, 0xA5, try_size);
    heap_caps_free(p);
  }
  Serial.println("PSRAM test done.");

  dm = new DisplayManager();
  if (!dm) {
    Serial.println("Failed to allocate DisplayManager");
    while (1) delay(1000);
  }
  if (!dm->initialize()) {
    Serial.println("Display initialize failed");
    while (1) delay(1000);
  }
  Serial.println("Display initialized ok!");
  
  // Now let's draw something to confirm everything works!
  auto lcd = dm->getDisplay();
  // Rotation is set in DisplayManager::initialize()
  lcd->fillScreen(TFT_BLACK);
  lcd->setTextColor(TFT_WHITE);
  lcd->setTextSize(3);
  lcd->setCursor(50, 50);
  lcd->println("SUCCESS!");
  lcd->setTextSize(2);
  lcd->setCursor(50, 100);
  lcd->println("LovyanGFX 1.1.16 works!");
  lcd->setCursor(50, 140);
  lcd->println("Elecrow 5\" HMI Display");
  
  // Draw some shapes
  lcd->drawRect(50, 200, 700, 200, TFT_GREEN);
  lcd->fillCircle(400, 300, 50, TFT_RED);
  lcd->drawLine(100, 450, 700, 450, TFT_CYAN);
  
  Serial.println("Drawing complete!");
}

void loop() {
  // Multi-combo test: iterate pclk polarity and write frequency and show RED/GREEN/BLUE
  auto lcd = dm->getDisplay();
  if (!lcd) {
    delay(1000);
    return;
  }

  // Access LGFX internals (LGFX class defined in DisplayManager.h)
  LGFX* lg = (LGFX*)lcd;
  auto& bus = lg->_bus_instance;
  auto& panel = lg->_panel_instance;

  const uint32_t freqs[] = {10000000u, 12000000u, 15000000u};
  const int pclk_vals[] = {0, 1};
  const uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  const char* colorNames[] = {"RED", "GREEN", "BLUE"};

  for (size_t pi = 0; pi < sizeof(pclk_vals)/sizeof(pclk_vals[0]); ++pi) {
    for (size_t fi = 0; fi < sizeof(freqs)/sizeof(freqs[0]); ++fi) {
      int pclk = pclk_vals[pi];
      uint32_t freq = freqs[fi];

      // Reconfigure bus
      auto cfg = bus.config();
      cfg.freq_write = freq;
      cfg.pclk_active_neg = pclk;
      bus.config(cfg);

      Serial.printf("--- TEST COMBO: pclk=%d freq=%u ---\n", pclk, freq);

      // Allow some time for the bus to settle
      delay(200);

      // Re-init the lcd device to ensure the new timing is applied cleanly
      lcd->init();

      for (size_t ci = 0; ci < 3; ++ci) {
        lcd->fillScreen(colors[ci]);
        Serial.printf("Showing %s (pclk=%d freq=%u)\n", colorNames[ci], pclk, freq);
        delay(1500);
      }
    }
  }

  Serial.println("Multi-combo smoke test complete. Halting further tests.");
  // Halt here - keep last color on screen
  while (1) delay(1000);
}
#endif // SMOKE_TEST
