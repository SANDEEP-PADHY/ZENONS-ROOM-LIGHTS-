#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "storage/StorageManager.h"

class RelayManager {
public:
    RelayManager();
    void begin(StorageManager* storage);

    bool setRelay(uint8_t id, bool state);
    bool toggleRelay(uint8_t id);
    bool getRelayState(uint8_t id);
    RelayConfig getRelay(uint8_t id);

    void setAll(bool state);
    void setAllWarm(bool state);

    bool updateRelayConfig(uint8_t id, const char* name, int8_t gpio, bool activeLow, bool enabled);

    void serializeRelays(JsonArray arr);

private:
    StorageManager* storage;
    RelayConfig relays[MAX_RELAY_CHANNELS];
    portMUX_TYPE relayMutex = portMUX_INITIALIZER_UNLOCKED;

    void applyHardwareState(uint8_t idx);
};
