#pragma once
#include <Arduino.h>

// =============================================================================
// DEFAULT NETWORK CONFIGURATION
// =============================================================================
#define DEFAULT_WIFI_SSID       "Sandeep"
#define DEFAULT_WIFI_PASS       "GPUC560048"
#define DEFAULT_MDNS_HOSTNAME   "zenon"
#define DEFAULT_DEVICE_NAME     "zenon"

// Static IP Defaults (Configurable in NVS / Web UI)
#define DEFAULT_STATIC_IP       "192.168.1.50"
#define DEFAULT_GATEWAY         "192.168.1.1"
#define DEFAULT_SUBNET          "255.255.255.0"
#define DEFAULT_DNS             "192.168.1.1"
#define DEFAULT_USE_STATIC_IP   false

// =============================================================================
// RELAY HARDWARE CONFIGURATION
// =============================================================================
#define MAX_RELAY_CHANNELS      8

struct RelayConfig {
    uint8_t id;             // 1 to 8
    char name[32];          // User-customizable channel name
    int8_t gpio;            // Configurable GPIO pin
    bool activeLow;         // true = Active LOW (standard), false = Active HIGH
    bool enabled;           // Whether channel is active
    bool state;             // Current ON/OFF state
    char type[16];          // "warm" or "spare"
};

// Safe default GPIO placeholders for ESP32-S3 (can be remapped via web interface)
const RelayConfig DEFAULT_RELAYS[MAX_RELAY_CHANNELS] = {
    {1, "Left Wall",        4,  true, true,  false, "warm"},
    {2, "Back Wall Main",   5,  true, true,  false, "warm"},
    {3, "Spare Ch 3",       6,  true, false, false, "spare"},
    {4, "Behind Monitor",   7,  true, true,  false, "warm"},
    {5, "Under Table",      15, true, true,  false, "warm"},
    {6, "Right Wall / Bed", 16, true, true,  false, "warm"},
    {7, "Spare Ch 7",       17, true, false, false, "spare"},
    {8, "Spare Ch 8",       18, true, false, false, "spare"}
};

// =============================================================================
// ADDRESSABLE RGB LED STRIP CONFIGURATION (WLED STYLE)
// =============================================================================
#define DEFAULT_RGB_GPIO        48
#define DEFAULT_LED_COUNT       144
#define DEFAULT_MAX_CURRENT_MA  5000 // 5A Power Limiter
#define DEFAULT_RGB_BRIGHTNESS  210  // ~82%
#define DEFAULT_RGB_CHIPSET     "WS2812B"
#define DEFAULT_COLOR_ORDER     "GRB"

struct RgbConfig {
    bool power;
    uint8_t brightness;     // 0 - 255
    uint8_t r;
    uint8_t g;
    uint8_t b;
    char effect[32];        // "static", "rainbow", "aurora", "fire", etc.
    uint8_t speed;          // 1 - 255
    uint8_t intensity;      // 1 - 255
    char chipset[16];       // "WS2812B", "WS2811", "SK6812"
    uint16_t ledCount;      // Configurable count (default 144)
    int8_t gpio;            // Data pin
    char colorOrder[8];     // "GRB", "RGB", "BGR", etc.
    uint16_t maxCurrent_mA; // Power limiting
};
