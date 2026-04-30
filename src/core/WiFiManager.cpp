// src/core/WiFiManager.cpp
#include "core/WiFiManager.h"
#include "config/Config.h"
#include <time.h>
#include <esp_netif.h>

namespace core {

WiFiManager::WiFiManager()
    : lastReconnectAttemptMs_(0)
    , lastDnsRefreshMs_(0)
    , reconnectedFlag_(false) {}

bool WiFiManager::connect() {
    // Register once: re-apply Cloudflare DNS immediately after every DHCP IP event.
    // DHCP renewals trigger ARDUINO_EVENT_WIFI_STA_GOT_IP and overwrite DNS —
    // catching that event here prevents the 7-second window where DNS is broken.
    static bool dhcpHandlerSet = false;
    if (!dhcpHandlerSet) {
        WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
            esp_netif_t* ni = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!ni) return;
            esp_netif_dns_info_t d = {};
            d.ip.type = ESP_IPADDR_TYPE_V4;
            d.ip.u_addr.ip4.addr = 0x01010101;  // 1.1.1.1
            esp_netif_set_dns_info(ni, ESP_NETIF_DNS_MAIN, &d);
            Serial.println("[DNS] DHCP event: Cloudflare 1.1.1.1 re-applied");
        }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
        dhcpHandlerSet = true;
    }

    Serial.printf("[WiFi] Connecting to SSID: \"%s\"\n", Config::WIFI_SSID);
    WiFi.persistent(false);    // don't write credentials to NVS — flash writes disable cache
    WiFi.disconnect(false);    // send proper deauth to AP before re-associating
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - started) > Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(300);
    }

    WiFi.setSleep(false);

    Serial.printf("[WiFi] Connected: %s  GW: %s  DNS: %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str());

    // TCP reachability probe — distinguishes broken ESP32 TCP stack from router/ISP filtering.
    // Router:80 is on LAN — if this fails, the TCP stack itself is broken.
    // If it succeeds but internet hosts fail, the router is filtering this device.
    {
        WiFiClient tcp;
        tcp.setTimeout(3);  // 3 seconds
        bool routerOk = tcp.connect(WiFi.gatewayIP(), 80);
        Serial.printf("[TCP] router %s:80 → %s  heap: %lu B free\n",
                      WiFi.gatewayIP().toString().c_str(),
                      routerOk ? "OK" : "FAIL",
                      (unsigned long)ESP.getFreeHeap());
        if (routerOk) tcp.stop();
    }

    applyCloudfareDns();

    // Pre-resolve every API hostname before configTime() starts SNTP.
    // SNTP triggers a concurrent DNS query for pool.ntp.org that races with the first
    // HTTP call and can leave auth.opensky-network.org timing out for 7s. Resolving
    // here first caches the IPs in lwIP — subsequent hostByName() calls are instant
    // cache hits even if DNS becomes temporarily unavailable.
    {
        IPAddress ip;
        const char* hosts[] = {
            "pool.ntp.org",
            "api.openweathermap.org",
            "opensky-network.org",
            "auth.opensky-network.org",
        };
        for (const char* h : hosts) {
            bool ok = WiFi.hostByName(h, ip);
            Serial.printf("[DNS] pre-resolve %-32s → %s\n", h, ok ? ip.toString().c_str() : "FAIL");
        }
    }

    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    return true;
}

void WiFiManager::applyCloudfareDns() {
    // dns_setserver() (lwIP-level) gets silently overwritten by ESP-IDF's DHCP client
    // on IP renewal events. esp_netif_set_dns_info() operates at the correct ESP-IDF
    // netif layer so it persists across renewals (and also updates lwIP underneath).
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        Serial.println("[DNS] ERROR: WIFI_STA_DEF netif not found");
        return;
    }
    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = 0x01010101;  // 1.1.1.1 in network byte order
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    lastDnsRefreshMs_ = millis();
    Serial.println("[DNS] Cloudflare 1.1.1.1 applied (esp_netif)");
}

void WiFiManager::tick(uint32_t nowMs) {
    if (WiFi.status() == WL_CONNECTED) {
        // Proactive 30s DNS re-apply removed — the DHCP event handler registered in
        // connect() re-applies Cloudflare DNS immediately on every ARDUINO_EVENT_WIFI_STA_GOT_IP.
        // The timer was firing 1–2s before every aircraft fetch, coinciding with the
        // esp_netif IPC call (tcpip_api_call) that briefly disturbs the AXI bus and
        // causes a 30-second periodic display glitch on the RGB parallel bus.
        return;
    }

    if ((nowMs - lastReconnectAttemptMs_) < Config::WIFI_RECONNECT_INTERVAL) {
        return;
    }

    lastReconnectAttemptMs_ = nowMs;
    Serial.println("[WiFi] Disconnected — attempting reconnect");

    if (connect()) {
        reconnectedFlag_ = true;
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::localIP() const {
    return WiFi.localIP().toString();
}

bool WiFiManager::justReconnected() {
    if (reconnectedFlag_) {
        reconnectedFlag_ = false;
        return true;
    }
    return false;
}

}  // namespace core
