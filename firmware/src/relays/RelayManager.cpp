#include "RelayManager.h"

RelayManager::RelayManager() : storage(nullptr) {}

void RelayManager::begin(StorageManager* storageMgr) {
    storage = storageMgr;
    if (storage) {
        storage->loadRelays(relays);
    } else {
        for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
            relays[i] = DEFAULT_RELAYS[i];
        }
    }

    // Initialize GPIO pins safely
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (relays[i].gpio >= 0 && relays[i].gpio <= 48) {
            applyHardwareState(i);
        }
    }
}

void RelayManager::applyHardwareState(uint8_t idx) {
    if (idx >= MAX_RELAY_CHANNELS) return;
    int8_t pin = relays[idx].gpio;
    if (pin < 0 || pin > 48) return;

    bool activeLow = relays[idx].activeLow;
    bool isOn = relays[idx].state && relays[idx].enabled;

    if (activeLow) {
        if (isOn) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);  // Sink to GND -> Relay turns ON
        } else {
            pinMode(pin, INPUT);     // Tri-state High-Z -> Zero current through optocoupler -> Relay 100% OFF
        }
    } else {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, isOn ? HIGH : LOW);
    }
}

bool RelayManager::setRelay(uint8_t id, bool state) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return false;
    uint8_t idx = id - 1;

    portENTER_CRITICAL(&relayMutex);
    relays[idx].state = state;
    applyHardwareState(idx);
    portEXIT_CRITICAL(&relayMutex);

    if (storage) {
        storage->saveRelayState(id, state);
    }
    return true;
}

bool RelayManager::toggleRelay(uint8_t id) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return false;
    uint8_t idx = id - 1;
    return setRelay(id, !relays[idx].state);
}

bool RelayManager::getRelayState(uint8_t id) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return false;
    return relays[id - 1].state;
}

RelayConfig RelayManager::getRelay(uint8_t id) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return DEFAULT_RELAYS[0];
    return relays[id - 1];
}

void RelayManager::setAll(bool state) {
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (relays[i].enabled) {
            setRelay(relays[i].id, state);
        }
    }
}

void RelayManager::setAllWarm(bool state) {
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (relays[i].enabled && strcmp(relays[i].type, "warm") == 0) {
            setRelay(relays[i].id, state);
        }
    }
}

bool RelayManager::updateRelayConfig(uint8_t id, const char* name, int8_t gpio, bool activeLow, bool enabled) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return false;
    uint8_t idx = id - 1;

    int8_t oldGpio = relays[idx].gpio;

    portENTER_CRITICAL(&relayMutex);
    if (name && strlen(name) > 0) {
        strncpy(relays[idx].name, name, sizeof(relays[idx].name) - 1);
    }
    relays[idx].gpio = gpio;
    relays[idx].activeLow = activeLow;
    relays[idx].enabled = enabled;
    portEXIT_CRITICAL(&relayMutex);

    if (oldGpio != gpio && gpio >= 0 && gpio <= 48) {
        pinMode(gpio, OUTPUT);
    }
    applyHardwareState(idx);

    if (storage) {
        storage->saveRelay(relays[idx]);
    }
    return true;
}

void RelayManager::serializeRelays(JsonArray arr) {
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = relays[i].id;
        obj["name"] = relays[i].name;
        obj["gpio"] = relays[i].gpio;
        obj["activeLow"] = relays[i].activeLow;
        obj["enabled"] = relays[i].enabled;
        obj["state"] = relays[i].state;
        obj["type"] = relays[i].type;
    }
}
