#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include "Config.h"
#include "storage/StorageManager.h"
#include "relays/RelayManager.h"
#include "rgb/RgbManager.h"
#include "api/ApiServer.h"

// System Singletons
StorageManager storageManager;
RelayManager relayManager;
RgbManager rgbManager;
ApiServer apiServer(80);

// Wi-Fi Connection State & Watchdog
uint32_t lastWifiCheckMs = 0;
uint32_t lastHeartbeatMs = 0;
String currentSsid;
String currentPass;
String currentHostname;
bool useStaticIp = false;
String staticIp, gateway, subnet, dns;

void setupWiFi() {
    Serial.println("\n[WiFi] Initializing Wi-Fi connection...");

    storageManager.loadNetworkConfig(currentSsid, currentPass, currentHostname, useStaticIp, staticIp, gateway, subnet, dns);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false); // Disable Wi-Fi power saving for ultra-low latency WebSocket

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.println("\n[WiFi] IP Event: Connected!");
            Serial.printf("[WiFi] IP Address : %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] Signal RSSI: %d dBm\n", WiFi.RSSI());
            
            // Start or restart MDNS
            if (MDNS.begin(currentHostname.c_str())) {
                MDNS.addService("http", "tcp", 80);
                Serial.printf("[mDNS] Hostname active: http://%s.local\n", currentHostname.c_str());
            }
        }
    });

    WiFi.disconnect(true);
    delay(100);

    WiFi.setHostname(currentHostname.c_str());

    if (useStaticIp) {
        IPAddress ipAddr, gwAddr, subAddr, dnsAddr;
        if (ipAddr.fromString(staticIp) && gwAddr.fromString(gateway) && subAddr.fromString(subnet) && dnsAddr.fromString(dns)) {
            WiFi.config(ipAddr, gwAddr, subAddr, dnsAddr);
            Serial.printf("[WiFi] Configured Static IP: %s (GW: %s, Sub: %s, DNS: %s)\n", 
                staticIp.c_str(), gateway.c_str(), subnet.c_str(), dns.c_str());
        }
    }

    Serial.println("[WiFi] Scanning available 2.4GHz networks...");
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] Found %d networks:\n", n);
    int targetChannel = 0;
    for (int i = 0; i < n; ++i) {
        Serial.printf("  [%d] SSID: '%s' | RSSI: %d dBm | Ch: %d | Enc: %d\n", 
            i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i));
        if (WiFi.SSID(i) == currentSsid) {
            targetChannel = WiFi.channel(i);
        }
    }

    WiFi.setHostname(currentHostname.c_str());

    if (targetChannel > 0) {
        Serial.printf("[WiFi] Found target network '%s' on Channel %d. Connecting...\n", currentSsid.c_str(), targetChannel);
        WiFi.begin(currentSsid.c_str(), currentPass.c_str(), targetChannel);
    } else {
        Serial.printf("[WiFi] Target '%s' not in immediate scan, attempting general connection...\n", currentSsid.c_str());
        WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    }

    Serial.printf("[WiFi] Connecting to SSID: '%s' ...", currentSsid.c_str());
    uint8_t timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 30) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected successfully!");
        Serial.printf("[WiFi] IP Address : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Signal RSSI: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("\n[WiFi] Connecting in background...");
    }
}

void setup() {
    // 1. Initialize Serial
    Serial.begin(115200);
    delay(1000);

    Serial.println("==================================================");
    Serial.println("    ZENON SMART ROOM LIGHTING CONTROLLER");
    Serial.println("         ESP32-S3 N16R8 PRODUCTION FIRMWARE");
    Serial.println("==================================================");

    // 2. Initialize Persistent Storage (NVS)
    Serial.print("[Storage] Initializing NVS Preferences... ");
    if (storageManager.begin()) {
        Serial.println("OK");
    } else {
        Serial.println("FAILED");
    }

    // 3. Initialize 8-Channel Relay Module
    Serial.print("[Relays] Initializing 8-Channel Relay Manager... ");
    relayManager.begin(&storageManager);
    Serial.println("OK");

    // 4. Initialize Addressable RGB Strip (144 LED/m WLED Engine)
    Serial.print("[RGB] Initializing Addressable RGB Controller... ");
    rgbManager.begin(&storageManager);
    Serial.println("OK");

    // 5. Mount LittleFS Filesystem
    Serial.print("[FS] Mounting LittleFS filesystem... ");
    if (LittleFS.begin(true)) {
        Serial.printf("OK (Total: %u KB, Used: %u KB)\n", LittleFS.totalBytes() / 1024, LittleFS.usedBytes() / 1024);
    } else {
        Serial.println("WARNING: LittleFS mount failed, using embedded PROGMEM assets.");
    }

    // 6. Connect to Wi-Fi & Start mDNS
    setupWiFi();

    // 7. Start Web Server, REST API & WebSockets
    Serial.print("[Web] Starting Async Web Server & WebSocket... ");
    apiServer.begin(&relayManager, &rgbManager, &storageManager);
    Serial.println("OK");

    Serial.println("--------------------------------------------------");
    Serial.printf(" Web UI available at: http://%s.local\n", DEFAULT_MDNS_HOSTNAME);
    Serial.printf(" Direct IP Address  : http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("--------------------------------------------------");
}

void loop() {
    // Web Server tick
    apiServer.loop();

    uint32_t now = millis();

    // Wi-Fi Auto-Reconnect Watchdog (every 10s)
    if (now - lastWifiCheckMs > 10000) {
        lastWifiCheckMs = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Lost connection. Reconnecting...");
            WiFi.disconnect();
            WiFi.begin(currentSsid.c_str(), currentPass.c_str());
        }
    }

    // Periodic Heartbeat Status Log (every 30s)
    if (now - lastHeartbeatMs > 30000) {
        lastHeartbeatMs = now;
        Serial.printf("[System] Uptime: %lu s | Free Heap: %u KB | Free PSRAM: %u KB | RSSI: %d dBm\n",
            now / 1000,
            ESP.getFreeHeap() / 1024,
            ESP.getFreePsram() / 1024,
            WiFi.RSSI()
        );
    }

    delay(10);
}
