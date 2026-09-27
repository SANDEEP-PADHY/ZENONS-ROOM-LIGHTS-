#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "relays/RelayManager.h"
#include "rgb/RgbManager.h"
#include "storage/StorageManager.h"

class ApiServer {
public:
    ApiServer(uint16_t port = 80);
    void begin(RelayManager* relays, RgbManager* rgb, StorageManager* storage);
    void loop();

    void broadcastRelayUpdate(uint8_t channel, bool state);
    void broadcastRgbUpdate();
    void broadcastFullState();

private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    RelayManager* relayManager;
    RgbManager* rgbManager;
    StorageManager* storageManager;

    uint32_t lastWsCleanupMs;

    void setupStaticRoutes();
    void setupRestRoutes();
    void setupWebSocket();

    void handleWsMessage(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len);
    void sendFullStateToClient(AsyncWebSocketClient* client);

    void getSystemInfoJson(JsonObject obj);
};
