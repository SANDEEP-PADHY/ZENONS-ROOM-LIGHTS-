#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class StorageManager {
public:
    StorageManager();
    bool begin();
    
    // Relay NVS Operations
    void loadRelays(RelayConfig relays[MAX_RELAY_CHANNELS]);
    void saveRelay(const RelayConfig& relay);
    void saveRelayState(uint8_t id, bool state);
    void saveAllRelays(const RelayConfig relays[MAX_RELAY_CHANNELS]);

    // RGB NVS Operations
    void loadRgb(RgbConfig& rgb);
    void saveRgb(const RgbConfig& rgb);
    void saveRgbState(bool power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, const char* effect, uint8_t speed, uint8_t intensity);

    // Network & System NVS
    void loadNetworkConfig(String& ssid, String& pass, String& hostname, bool& useStatic, String& ip, String& gw, String& subnet, String& dns);
    void saveNetworkConfig(const String& ssid, const String& pass, const String& hostname, bool useStatic, const String& ip, const String& gw, const String& subnet, const String& dns);

    // Factory Reset
    void factoryReset();

private:
    Preferences prefs;
};
