#include "web/WebDashboard.h"

#include <ArduinoJson.h>

namespace web {

namespace {
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>Plane Tracker v4</title>
  <style>
    body { font-family: sans-serif; margin: 0; background: #0b1220; color: #e5edf9; }
    .wrap { max-width: 900px; margin: 0 auto; padding: 16px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(220px,1fr)); gap: 12px; }
    .card { background: #111a2c; border: 1px solid #26324a; border-radius: 10px; padding: 12px; }
    .title { color: #8cb2ff; font-size: 12px; text-transform: uppercase; letter-spacing: .08em; }
    .value { font-size: 28px; font-weight: 700; margin-top: 4px; }
    .small { color: #afc5e6; font-size: 14px; }
  </style>
</head>
<body>
  <div class='wrap'>
    <h1>Plane Tracker v4 Dashboard</h1>
    <div class='grid'>
      <div class='card'><div class='title'>Aircraft Nearby</div><div class='value' id='aircraft'>-</div></div>
      <div class='card'><div class='title'>Temperature</div><div class='value' id='temp'>-</div></div>
      <div class='card'><div class='title'>Weather</div><div class='value small' id='weather'>-</div></div>
      <div class='card'><div class='title'>Uptime</div><div class='value small' id='uptime'>-</div></div>
      <div class='card'><div class='title'>Heap</div><div class='value small' id='heap'>-</div></div>
      <div class='card'><div class='title'>WiFi RSSI</div><div class='value small' id='rssi'>-</div></div>
    </div>
  </div>
  <script>
    async function refresh() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        document.getElementById('aircraft').textContent = data.aircraft_count;
        document.getElementById('temp').textContent = data.temperature_f.toFixed(1) + ' F';
        document.getElementById('weather').textContent = data.condition + ' (' + data.description + ')';
        document.getElementById('uptime').textContent = data.uptime_sec + ' s';
        document.getElementById('heap').textContent = data.free_heap + ' bytes';
        document.getElementById('rssi').textContent = data.wifi_rssi + ' dBm';
      } catch (e) {
        console.log(e);
      }
    }
    refresh();
    setInterval(refresh, 3000);
  </script>
</body>
</html>
)HTML";
}

WebDashboard::WebDashboard()
    : server_(80)
    , aircraftCountCache_(0)
    , started_(false) {
}

bool WebDashboard::begin(uint16_t port) {
  (void)port;

    if (started_) {
        return true;
    }

    registerRoutes();
    server_.begin();
    started_ = true;
    return true;
}

void WebDashboard::loop() {
    if (!started_) {
        return;
    }
    server_.handleClient();
}

void WebDashboard::update(const core::HealthSnapshot& health, const WeatherData& weather, int aircraftCount) {
    healthCache_ = health;
    weatherCache_ = weather;
    aircraftCountCache_ = aircraftCount;
}

void WebDashboard::registerRoutes() {
    server_.on("/", HTTP_GET, [this]() { handleIndex(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatusJson(); });
}

void WebDashboard::handleIndex() {
    server_.send(200, "text/html", INDEX_HTML);
}

void WebDashboard::handleStatusJson() {
    JsonDocument doc;
    doc["uptime_sec"] = healthCache_.uptimeSec;
    doc["free_heap"] = healthCache_.freeHeap;
    doc["min_free_heap"] = healthCache_.minFreeHeap;
    doc["wifi_rssi"] = healthCache_.wifiRssi;
    doc["aircraft_count"] = aircraftCountCache_;

    doc["temperature_f"] = weatherCache_.temperature;
    doc["humidity"] = weatherCache_.humidity;
    doc["condition"] = weatherCache_.condition;
    doc["description"] = weatherCache_.description;

    String payload;
    serializeJson(doc, payload);
    server_.send(200, "application/json", payload);
}

}  // namespace web
