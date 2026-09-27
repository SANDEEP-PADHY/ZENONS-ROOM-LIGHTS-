#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NeoPixelBus.h>
#include "Config.h"
#include "storage/StorageManager.h"
#include "effects/EffectsEngine.h"

class RgbManager {
public:
    RgbManager();
    void begin(StorageManager* storage);

    void setPower(bool power);
    void setBrightness(uint8_t brightness);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setEffect(const char* effect);
    void setSpeed(uint8_t speed);
    void setIntensity(uint8_t intensity);
    void setState(bool power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, const char* effect, uint8_t speed, uint8_t intensity);

    bool updateHardwareConfig(const char* chipset, uint16_t ledCount, int8_t gpio, const char* colorOrder, uint16_t maxCurrent_mA);

    RgbConfig getConfig();
    void serializeRgb(JsonObject obj);

private:
    StorageManager* storage;
    RgbConfig config;
    NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip;
    EffectsEngine effects;

    portMUX_TYPE rgbMutex = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t rgbTaskHandle = nullptr;

    void initHardware();
    static void rgbTask(void* parameter);
};
