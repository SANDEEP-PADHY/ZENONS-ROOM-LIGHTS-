#include "RgbManager.h"

RgbManager::RgbManager() : storage(nullptr), strip(nullptr) {}

void RgbManager::begin(StorageManager* storageMgr) {
    storage = storageMgr;
    if (storage) {
        storage->loadRgb(config);
    } else {
        config.power = false;
        config.brightness = DEFAULT_RGB_BRIGHTNESS;
        config.r = 0;
        config.g = 240;
        config.b = 255;
        strncpy(config.effect, "aurora", sizeof(config.effect) - 1);
        config.speed = 128;
        config.intensity = 160;
        strncpy(config.chipset, DEFAULT_RGB_CHIPSET, sizeof(config.chipset) - 1);
        config.ledCount = DEFAULT_LED_COUNT;
        config.gpio = DEFAULT_RGB_GPIO;
        strncpy(config.colorOrder, DEFAULT_COLOR_ORDER, sizeof(config.colorOrder) - 1);
        config.maxCurrent_mA = DEFAULT_MAX_CURRENT_MA;
    }

    initHardware();

    // Start FreeRTOS animation task pinned to Core 1
    xTaskCreatePinnedToCore(
        rgbTask,
        "RgbTask",
        4096,
        this,
        2,
        &rgbTaskHandle,
        1 // Core 1 (Core 0 handles Wi-Fi and Web server)
    );
}

void RgbManager::initHardware() {
    if (strip) {
        delete strip;
        strip = nullptr;
    }

    strip = new NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>(config.ledCount, config.gpio);
    strip->Begin();
    strip->ClearTo(RgbColor(0, 0, 0));
    strip->Show();

    effects.begin(config.ledCount);
}

void RgbManager::rgbTask(void* parameter) {
    RgbManager* self = static_cast<RgbManager*>(parameter);
    while (true) {
        RgbConfig currentCfg = self->getConfig();
        if (self->strip) {
            self->effects.update(self->strip, currentCfg);
        }

        vTaskDelay(pdMS_TO_TICKS(15)); // ~66 FPS smooth animation tick
    }
}

void RgbManager::setPower(bool power) {
    portENTER_CRITICAL(&rgbMutex);
    config.power = power;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setBrightness(uint8_t brightness) {
    portENTER_CRITICAL(&rgbMutex);
    config.brightness = brightness;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setColor(uint8_t r, uint8_t g, uint8_t b) {
    portENTER_CRITICAL(&rgbMutex);
    config.r = r;
    config.g = g;
    config.b = b;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setEffect(const char* effect) {
    if (!effect) return;
    portENTER_CRITICAL(&rgbMutex);
    strncpy(config.effect, effect, sizeof(config.effect) - 1);
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setSpeed(uint8_t speed) {
    portENTER_CRITICAL(&rgbMutex);
    config.speed = speed;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setIntensity(uint8_t intensity) {
    portENTER_CRITICAL(&rgbMutex);
    config.intensity = intensity;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

void RgbManager::setState(bool power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, const char* effect, uint8_t speed, uint8_t intensity) {
    portENTER_CRITICAL(&rgbMutex);
    config.power = power;
    config.brightness = brightness;
    config.r = r;
    config.g = g;
    config.b = b;
    if (effect) strncpy(config.effect, effect, sizeof(config.effect) - 1);
    config.speed = speed;
    config.intensity = intensity;
    portEXIT_CRITICAL(&rgbMutex);

    if (storage) storage->saveRgbState(config.power, config.brightness, config.r, config.g, config.b, config.effect, config.speed, config.intensity);
}

bool RgbManager::updateHardwareConfig(const char* chipset, uint16_t ledCount, int8_t gpio, const char* colorOrder, uint16_t maxCurrent_mA) {
    portENTER_CRITICAL(&rgbMutex);
    if (chipset) strncpy(config.chipset, chipset, sizeof(config.chipset) - 1);
    if (ledCount > 0) config.ledCount = ledCount;
    if (gpio >= 0 && gpio <= 48) config.gpio = gpio;
    if (colorOrder) strncpy(config.colorOrder, colorOrder, sizeof(config.colorOrder) - 1);
    config.maxCurrent_mA = maxCurrent_mA;
    portEXIT_CRITICAL(&rgbMutex);

    initHardware();

    if (storage) storage->saveRgb(config);
    return true;
}

RgbConfig RgbManager::getConfig() {
    portENTER_CRITICAL(&rgbMutex);
    RgbConfig copy = config;
    portEXIT_CRITICAL(&rgbMutex);
    return copy;
}

void RgbManager::serializeRgb(JsonObject obj) {
    RgbConfig cfg = getConfig();
    obj["power"] = cfg.power;
    obj["brightness"] = cfg.brightness;
    JsonObject col = obj["color"].to<JsonObject>();
    col["r"] = cfg.r;
    col["g"] = cfg.g;
    col["b"] = cfg.b;
    obj["effect"] = cfg.effect;
    obj["speed"] = cfg.speed;
    obj["intensity"] = cfg.intensity;
    obj["chipset"] = cfg.chipset;
    obj["ledCount"] = cfg.ledCount;
    obj["gpio"] = cfg.gpio;
    obj["colorOrder"] = cfg.colorOrder;
    obj["maxCurrent_mA"] = cfg.maxCurrent_mA;
}
