#include "ApiServer.h"
#include "WebAssets.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>

ApiServer::ApiServer(uint16_t port)
    : server(port), ws("/ws"), relayManager(nullptr), rgbManager(nullptr),
      storageManager(nullptr), lastWsCleanupMs(0) {}

void ApiServer::begin(RelayManager* relays, RgbManager* rgb, StorageManager* storage) {
    relayManager = relays;
    rgbManager = rgb;
    storageManager = storage;

    setupWebSocket();
    setupRestRoutes();
    setupStaticRoutes();

    server.begin();
}

void ApiServer::loop() {
    uint32_t now = millis();
    if (now - lastWsCleanupMs > 2000) {
        ws.cleanupClients();
        lastWsCleanupMs = now;
    }
}

void ApiServer::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            sendFullStateToClient(client);
        } else if (type == WS_EVT_DATA) {
            this->handleWsMessage(client, arg, data, len);
        }
    });

    server.addHandler(&ws);
}

void ApiServer::handleWsMessage(AsyncWebSocketClient* client, void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err) return;

        const char* action = doc["action"] | "";

        if (strcmp(action, "getState") == 0) {
            sendFullStateToClient(client);
        } else if (strcmp(action, "setRelay") == 0) {
            uint8_t ch = doc["channel"] | 0;
            bool st = doc["state"] | false;
            if (relayManager && ch >= 1 && ch <= MAX_RELAY_CHANNELS) {
                relayManager->setRelay(ch, st);
                broadcastRelayUpdate(ch, st);
            }
        } else if (strcmp(action, "setRgb") == 0) {
            if (rgbManager) {
                if (doc["power"].is<bool>()) rgbManager->setPower(doc["power"].as<bool>());
                if (doc["brightness"].is<uint8_t>()) rgbManager->setBrightness(doc["brightness"].as<uint8_t>());
                if (doc["color"].is<JsonObject>()) {
                    JsonObject col = doc["color"];
                    rgbManager->setColor(col["r"] | 0, col["g"] | 0, col["b"] | 0);
                }
                if (doc["effect"].is<const char*>()) rgbManager->setEffect(doc["effect"].as<const char*>());
                if (doc["speed"].is<uint8_t>()) rgbManager->setSpeed(doc["speed"].as<uint8_t>());
                if (doc["intensity"].is<uint8_t>()) rgbManager->setIntensity(doc["intensity"].as<uint8_t>());

                broadcastRgbUpdate();
            }
        } else if (strcmp(action, "masterAll") == 0) {
            bool st = doc["state"] | false;
            if (relayManager) relayManager->setAll(st);
            if (rgbManager) rgbManager->setPower(st);
            broadcastFullState();
        } else if (strcmp(action, "masterAllWarm") == 0) {
            if (relayManager) relayManager->setAllWarm(true);
            broadcastFullState();
        } else if (strcmp(action, "masterAllOff") == 0) {
            if (relayManager) relayManager->setAll(false);
            if (rgbManager) rgbManager->setPower(false);
            broadcastFullState();
        } else if (strcmp(action, "updateRelayConfig") == 0) {
            uint8_t ch = doc["channel"] | 0;
            const char* name = doc["name"] | "";
            int8_t gpio = doc["gpio"] | -1;
            bool activeLow = doc["activeLow"] | true;
            bool enabled = doc["enabled"] | true;
            if (relayManager) {
                relayManager->updateRelayConfig(ch, name, gpio, activeLow, enabled);
                broadcastFullState();
            }
        } else if (strcmp(action, "updateRgbHardwareConfig") == 0) {
            const char* chip = doc["chipset"] | DEFAULT_RGB_CHIPSET;
            uint16_t count = doc["ledCount"] | DEFAULT_LED_COUNT;
            int8_t gpio = doc["gpio"] | DEFAULT_RGB_GPIO;
            const char* ord = doc["colorOrder"] | DEFAULT_COLOR_ORDER;
            uint16_t ma = doc["maxCurrent_mA"] | DEFAULT_MAX_CURRENT_MA;
            if (rgbManager) {
                rgbManager->updateHardwareConfig(chip, count, gpio, ord, ma);
                broadcastFullState();
            }
        } else if (strcmp(action, "restartEsp") == 0) {
            ESP.restart();
        }
    }
}

void ApiServer::sendFullStateToClient(AsyncWebSocketClient* client) {
    if (!client || !client->canSend()) return;

    JsonDocument doc;
    doc["type"] = "fullState";

    JsonArray relaysArr = doc["relays"].to<JsonArray>();
    if (relayManager) relayManager->serializeRelays(relaysArr);

    JsonObject rgbObj = doc["rgb"].to<JsonObject>();
    if (rgbManager) rgbManager->serializeRgb(rgbObj);

    JsonObject sysObj = doc["system"].to<JsonObject>();
    getSystemInfoJson(sysObj);

    String output;
    serializeJson(doc, output);
    client->text(output);
}

void ApiServer::broadcastRelayUpdate(uint8_t channel, bool state) {
    if (ws.count() == 0) return;
    JsonDocument doc;
    doc["type"] = "relayUpdate";
    doc["channel"] = channel;
    doc["state"] = state;

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void ApiServer::broadcastRgbUpdate() {
    if (ws.count() == 0) return;
    JsonDocument doc;
    doc["type"] = "rgbUpdate";
    if (rgbManager) {
        RgbConfig cfg = rgbManager->getConfig();
        doc["power"] = cfg.power;
        doc["brightness"] = cfg.brightness;
        JsonObject col = doc["color"].to<JsonObject>();
        col["r"] = cfg.r;
        col["g"] = cfg.g;
        col["b"] = cfg.b;
        doc["effect"] = cfg.effect;
        doc["speed"] = cfg.speed;
        doc["intensity"] = cfg.intensity;
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void ApiServer::broadcastFullState() {
    if (ws.count() == 0) return;
    JsonDocument doc;
    doc["type"] = "fullState";

    JsonArray relaysArr = doc["relays"].to<JsonArray>();
    if (relayManager) relayManager->serializeRelays(relaysArr);

    JsonObject rgbObj = doc["rgb"].to<JsonObject>();
    if (rgbManager) rgbManager->serializeRgb(rgbObj);

    JsonObject sysObj = doc["system"].to<JsonObject>();
    getSystemInfoJson(sysObj);

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void ApiServer::getSystemInfoJson(JsonObject obj) {
    obj["device"] = DEFAULT_DEVICE_NAME;
    obj["ip"] = WiFi.localIP().toString();
    obj["hostname"] = String(DEFAULT_MDNS_HOSTNAME) + ".local";
    obj["rssi"] = WiFi.RSSI();
    obj["uptime"] = millis() / 1000;
    obj["heap"] = ESP.getFreeHeap();
    obj["psram"] = ESP.getFreePsram();
    obj["version"] = "v1.0.0-PROD";
}

void ApiServer::setupRestRoutes() {
    // GET /api/state -> Full State
    server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray relaysArr = doc["relays"].to<JsonArray>();
        if (relayManager) relayManager->serializeRelays(relaysArr);

        JsonObject rgbObj = doc["rgb"].to<JsonObject>();
        if (rgbManager) rgbManager->serializeRgb(rgbObj);

        JsonObject sysObj = doc["system"].to<JsonObject>();
        getSystemInfoJson(sysObj);

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // POST /api/relay/:id/:action (on, off, toggle)
    server.on("^\\/api\\/relay\\/([1-8])\\/(on|off|toggle)$", HTTP_POST, [this](AsyncWebServerRequest* request) {
        String chStr = request->pathArg(0);
        String actStr = request->pathArg(1);
        uint8_t ch = chStr.toInt();

        if (relayManager && ch >= 1 && ch <= MAX_RELAY_CHANNELS) {
            bool targetState = false;
            if (actStr == "on") targetState = true;
            else if (actStr == "off") targetState = false;
            else if (actStr == "toggle") targetState = !relayManager->getRelayState(ch);

            relayManager->setRelay(ch, targetState);
            broadcastRelayUpdate(ch, targetState);

            JsonDocument res;
            res["success"] = true;
            res["channel"] = ch;
            res["state"] = targetState;
            String out;
            serializeJson(res, out);
            request->send(200, "application/json", out);
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid channel\"}");
        }
    });

    // POST /api/master/:action (all-on, all-off, all-warm, rgb-toggle)
    server.on("^\\/api\\/master\\/(all-on|all-off|all-warm|rgb-toggle)$", HTTP_POST, [this](AsyncWebServerRequest* request) {
        String act = request->pathArg(0);
        if (act == "all-on") {
            if (relayManager) relayManager->setAll(true);
            if (rgbManager) rgbManager->setPower(true);
        } else if (act == "all-off") {
            if (relayManager) relayManager->setAll(false);
            if (rgbManager) rgbManager->setPower(false);
        } else if (act == "all-warm") {
            if (relayManager) relayManager->setAllWarm(true);
        } else if (act == "rgb-toggle") {
            if (rgbManager) {
                RgbConfig cfg = rgbManager->getConfig();
                rgbManager->setPower(!cfg.power);
            }
        }
        broadcastFullState();
        request->send(200, "application/json", "{\"success\":true}");
    });

    // GET /api/rgb
    server.on("/api/rgb", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (rgbManager) rgbManager->serializeRgb(doc.to<JsonObject>());
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // POST /api/system/restart
    server.on("/api/system/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"rebooting\":true}");
        delay(500);
        ESP.restart();
    });
}

void ApiServer::setupStaticRoutes() {
    // Serve from LittleFS if mounted and file exists, otherwise fallback to embedded PROGMEM WebAssets.h

    // Root / or /index.html
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, index_html_mime, index_html_gz, index_html_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, index_html_mime, index_html_gz, index_html_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    // CSS
    server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/css/style.css")) {
            request->send(LittleFS, "/css/style.css", "text/css");
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, css_style_css_mime, css_style_css_gz, css_style_css_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    // JS
    server.on("/js/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/js/app.js")) {
            request->send(LittleFS, "/js/app.js", "application/javascript");
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, js_app_js_mime, js_app_js_gz, js_app_js_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    // SVG Room
    server.on("/room/room.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/room/room.svg")) {
            request->send(LittleFS, "/room/room.svg", "image/svg+xml");
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, room_room_svg_mime, room_room_svg_gz, room_room_svg_len);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
    });

    // 404 Fallback
    server.onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404, "text/plain", "Not Found");
        }
    });
}
