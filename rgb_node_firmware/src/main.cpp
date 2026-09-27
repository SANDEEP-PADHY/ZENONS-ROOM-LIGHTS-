#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <FastLED.h>

// ==============================================================================
// HARDWARE & NETWORK CONFIGURATION
// ==============================================================================
#define LED_COUNT       144
#define PIN_PRIMARY     48  // Primary GPIO 48
#define PIN_SECONDARY   4   // Secondary GPIO 4 (fallback)
#define PIN_TERTIARY    38  // Tertiary GPIO 38 (fallback)

const char* WIFI_SSID   = "Sandeep";
const char* WIFI_PASS   = "GPUC560048";
const uint16_t UDP_PORT = 4048;

CRGB ledsPrimary[LED_COUNT];
CRGB ledsSecondary[LED_COUNT];

WiFiUDP udp;
uint8_t packetBuffer[1024];

// Current State
bool pwr = true;
uint8_t bri = 255;
uint8_t r = 0, g = 240, b = 255;
char effect[32] = "rainbow";
uint8_t spd = 180;
uint8_t intens = 180;

uint32_t stepCounter = 0;
uint32_t lastFrameMs = 0;

void setupWiFi() {
    Serial.println("\n[Node] Connecting to Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint8_t timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 30) {
        delay(300);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[Node] Connected to Wi-Fi!");
        Serial.printf("[Node] IP Address: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin("zenon-rgb")) {
            MDNS.addService("http", "tcp", 80);
            Serial.println("[Node] mDNS active: http://zenon-rgb.local");
        }
    } else {
        Serial.println("\n[Node] Wi-Fi connecting in background...");
    }

    udp.begin(UDP_PORT);
    Serial.printf("[Node] UDP listener active on port %d\n", UDP_PORT);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("==================================================");
    Serial.println("   ZENON RGB STRIP NODE (FASTLED + UDP SYNC)");
    Serial.println("==================================================");

    // Initialize FastLED on GPIO 48, GPIO 4, and GPIO 38
    FastLED.addLeds<WS2812B, PIN_PRIMARY, GRB>(ledsPrimary, LED_COUNT);
    FastLED.addLeds<WS2812B, PIN_SECONDARY, GRB>(ledsSecondary, LED_COUNT);
    FastLED.setBrightness(255);

    // Initial Test Flash (Red -> Green -> Blue -> Rainbow)
    fill_solid(ledsPrimary, LED_COUNT, CRGB::Red);
    fill_solid(ledsSecondary, LED_COUNT, CRGB::Red);
    FastLED.show();
    delay(300);

    fill_solid(ledsPrimary, LED_COUNT, CRGB::Green);
    fill_solid(ledsSecondary, LED_COUNT, CRGB::Green);
    FastLED.show();
    delay(300);

    fill_solid(ledsPrimary, LED_COUNT, CRGB::Blue);
    fill_solid(ledsSecondary, LED_COUNT, CRGB::Blue);
    FastLED.show();
    delay(300);

    setupWiFi();
}

void handleUdp() {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0) {
            packetBuffer[len] = 0;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, packetBuffer);
            if (!err) {
                if (!doc["pwr"].isNull()) pwr = doc["pwr"].as<bool>();
                if (!doc["bri"].isNull()) bri = doc["bri"].as<uint8_t>();
                if (!doc["r"].isNull()) r = doc["r"].as<uint8_t>();
                if (!doc["g"].isNull()) g = doc["g"].as<uint8_t>();
                if (!doc["b"].isNull()) b = doc["b"].as<uint8_t>();
                if (!doc["eff"].isNull()) {
                    const char* eff = doc["eff"].as<const char*>();
                    if (eff) strncpy(effect, eff, sizeof(effect) - 1);
                }
                if (!doc["spd"].isNull()) spd = doc["spd"].as<uint8_t>();
                if (!doc["int"].isNull()) intens = doc["int"].as<uint8_t>();

                Serial.printf("[Node UDP] State: pwr=%d bri=%d eff=%s color=(%d,%d,%d)\n", pwr, bri, effect, r, g, b);
            }
        }
    }
}

void renderEffects() {
    if (!pwr || bri == 0) {
        FastLED.clear();
        FastLED.show();
        return;
    }

    FastLED.setBrightness(bri);

    uint32_t delayMs = map(spd, 1, 255, 60, 5);
    uint32_t now = millis();
    if (now - lastFrameMs < delayMs) {
        return;
    }
    lastFrameMs = now;
    stepCounter++;

    if (strcmp(effect, "static") == 0) {
        fill_solid(ledsPrimary, LED_COUNT, CRGB(r, g, b));
    } else if (strcmp(effect, "rainbow") == 0) {
        fill_rainbow(ledsPrimary, LED_COUNT, stepCounter & 0xFF, 256 / LED_COUNT);
    } else if (strcmp(effect, "breathe") == 0) {
        uint8_t sinVal = beatsin8(spd / 4, 30, 255);
        fill_solid(ledsPrimary, LED_COUNT, CRGB(r, g, b));
        FastLED.setBrightness((uint8_t)(((uint16_t)bri * sinVal) >> 8));
    } else if (strcmp(effect, "chase") == 0) {
        FastLED.clear();
        uint16_t head = stepCounter % LED_COUNT;
        uint8_t tail = map(intens, 1, 255, 5, 25);
        for (uint8_t i = 0; i < tail; i++) {
            int idx = head - i;
            if (idx >= 0 && idx < LED_COUNT) {
                uint8_t fade = 255 - (i * (255 / tail));
                ledsPrimary[idx] = CRGB(r, g, b);
                ledsPrimary[idx].nscale8_video(fade);
            }
        }
    } else if (strcmp(effect, "fire") == 0) {
        static byte heat[LED_COUNT];
        for (int i = 0; i < LED_COUNT; i++) {
            heat[i] = qsub8(heat[i], random8(0, ((50 * 10) / LED_COUNT) + 2));
        }
        for (int k = LED_COUNT - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        if (random8() < 160) {
            int y = random8(7);
            heat[y] = qadd8(heat[y], random8(160, 255));
        }
        for (int j = 0; j < LED_COUNT; j++) {
            CRGB color = HeatColor(heat[j]);
            ledsPrimary[j] = color;
        }
    } else if (strcmp(effect, "aurora") == 0) {
        float time = (float)stepCounter * 0.03f;
        for (int i = 0; i < LED_COUNT; i++) {
            float x = (float)i / (float)LED_COUNT * 3.0f;
            float val = (sin(x * 2.0f + time) + sin(x * 4.0f - time * 0.7f)) / 2.0f;
            uint8_t norm = (uint8_t)((val + 1.0f) * 127.5f);
            ledsPrimary[i] = (norm < 128) ? blend(CRGB(0, 240, 255), CRGB(46, 204, 113), norm * 2)
                                         : blend(CRGB(46, 204, 113), CRGB(155, 89, 182), (norm - 128) * 2);
        }
    } else {
        fill_solid(ledsPrimary, LED_COUNT, CRGB(r, g, b));
    }

    // Mirror to secondary pin
    memcpy(ledsSecondary, ledsPrimary, sizeof(ledsPrimary));

    FastLED.show();
}

void loop() {
    handleUdp();
    renderEffects();
    delay(5);
}
