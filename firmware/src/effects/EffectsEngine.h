#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>
#include "Config.h"

class EffectsEngine {
public:
    EffectsEngine();
    void begin(uint16_t count);
    void setLedCount(uint16_t count);

    // Main frame tick (non-blocking, updates pixel buffer)
    void update(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);

private:
    uint16_t numLeds;
    uint32_t stepCounter;
    uint32_t lastUpdateMs;

    // Internal state buffers for dynamic effects
    uint8_t* heatBuffer; // for Fire effect
    uint8_t* twinkleBuffer; // for Twinkle effect

    // Individual Effect Implementations
    void renderStatic(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderRainbow(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderColorWipe(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderChase(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderBreathe(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderFire(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderTwinkle(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderTheaterChase(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderGradient(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderPolice(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderAurora(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderMeteor(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);
    void renderRandomColors(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config);

    // Power / Current Limiting
    void applyPowerLimiter(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, uint16_t maxCurrent_mA);

    // Color Helpers
    RgbColor wheel(uint8_t pos);
    RgbColor scaleColor(const RgbColor& color, uint8_t scale);
    uint8_t sin8(uint8_t angle);
};
