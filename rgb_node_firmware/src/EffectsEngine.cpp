#include "EffectsEngine.h"

EffectsEngine::EffectsEngine() 
    : numLeds(144), stepCounter(0), lastUpdateMs(0),
      heatBuffer(nullptr), twinkleBuffer(nullptr) {}

void EffectsEngine::begin(uint16_t count) {
    setLedCount(count);
}

void EffectsEngine::setLedCount(uint16_t count) {
    numLeds = count > 0 ? count : 144;
    if (heatBuffer) delete[] heatBuffer;
    if (twinkleBuffer) delete[] twinkleBuffer;

    heatBuffer = new uint8_t[numLeds]();
    twinkleBuffer = new uint8_t[numLeds]();
}

RgbColor EffectsEngine::scaleColor(const RgbColor& color, uint8_t scale) {
    return RgbColor(
        (uint8_t)(((uint16_t)color.R * scale) >> 8),
        (uint8_t)(((uint16_t)color.G * scale) >> 8),
        (uint8_t)(((uint16_t)color.B * scale) >> 8)
    );
}

RgbColor EffectsEngine::wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return RgbColor(255 - pos * 3, 0, pos * 3);
    } else if (pos < 170) {
        pos -= 85;
        return RgbColor(0, pos * 3, 255 - pos * 3);
    } else {
        pos -= 170;
        return RgbColor(pos * 3, 255 - pos * 3, 0);
    }
}

uint8_t EffectsEngine::sin8(uint8_t angle) {
    float rad = (float)angle * (2.0f * 3.14159265f / 256.0f);
    return (uint8_t)((sin(rad) + 1.0f) * 127.5f);
}

void EffectsEngine::update(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    if (!strip) return;

    if (!config.power) {
        strip->ClearTo(RgbColor(0, 0, 0));
        strip->Show();
        return;
    }

    uint32_t delayMs = map(config.speed, 1, 255, 60, 5);
    uint32_t now = millis();
    if (now - lastUpdateMs < delayMs) {
        return;
    }
    lastUpdateMs = now;
    stepCounter++;

    const char* effect = config.effect;

    if (strcmp(effect, "static") == 0) {
        renderStatic(strip, config);
    } else if (strcmp(effect, "rainbow") == 0) {
        renderRainbow(strip, config);
    } else if (strcmp(effect, "wipe") == 0) {
        renderColorWipe(strip, config);
    } else if (strcmp(effect, "chase") == 0) {
        renderChase(strip, config);
    } else if (strcmp(effect, "breathe") == 0) {
        renderBreathe(strip, config);
    } else if (strcmp(effect, "fire") == 0) {
        renderFire(strip, config);
    } else if (strcmp(effect, "twinkle") == 0) {
        renderTwinkle(strip, config);
    } else if (strcmp(effect, "theater") == 0) {
        renderTheaterChase(strip, config);
    } else if (strcmp(effect, "gradient") == 0) {
        renderGradient(strip, config);
    } else if (strcmp(effect, "police") == 0) {
        renderPolice(strip, config);
    } else if (strcmp(effect, "aurora") == 0) {
        renderAurora(strip, config);
    } else if (strcmp(effect, "meteor") == 0) {
        renderMeteor(strip, config);
    } else if (strcmp(effect, "random") == 0) {
        renderRandomColors(strip, config);
    } else {
        renderStatic(strip, config);
    }

    applyPowerLimiter(strip, config.maxCurrent_mA);
    strip->Show();
}

void EffectsEngine::renderStatic(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);
    strip->ClearTo(scaled);
}

void EffectsEngine::renderRainbow(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    uint8_t step = stepCounter & 0xFF;
    for (uint16_t i = 0; i < numLeds; i++) {
        uint8_t pos = ((i * 256 / numLeds) + step) & 0xFF;
        RgbColor col = scaleColor(wheel(pos), config.brightness);
        strip->SetPixelColor(i, col);
    }
}

void EffectsEngine::renderColorWipe(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    uint16_t head = stepCounter % (numLeds * 2);
    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);

    for (uint16_t i = 0; i < numLeds; i++) {
        if (head < numLeds) {
            strip->SetPixelColor(i, i <= head ? scaled : RgbColor(0, 0, 0));
        } else {
            uint16_t clearIdx = head - numLeds;
            strip->SetPixelColor(i, i <= clearIdx ? RgbColor(0, 0, 0) : scaled);
        }
    }
}

void EffectsEngine::renderChase(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);
    strip->ClearTo(RgbColor(0, 0, 0));

    uint8_t segLen = map(config.intensity, 1, 255, 4, 20);
    uint16_t head = stepCounter % numLeds;

    for (uint8_t j = 0; j < segLen; j++) {
        int idx = head - j;
        if (idx >= 0 && idx < numLeds) {
            uint8_t fade = 255 - (j * (255 / segLen));
            strip->SetPixelColor(idx, scaleColor(scaled, fade));
        }
    }
}

void EffectsEngine::renderBreathe(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    float rad = (stepCounter % 360) * (3.14159f / 180.0f);
    float factor = (sin(rad) + 1.0f) / 2.0f;
    uint8_t effectiveBri = (uint8_t)(config.brightness * (0.15f + 0.85f * factor));

    RgbColor base(config.r, config.g, config.b);
    RgbColor col = scaleColor(base, effectiveBri);
    strip->ClearTo(col);
}

void EffectsEngine::renderFire(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    if (!heatBuffer) return;

    uint8_t cooling = map(config.intensity, 1, 255, 20, 80);
    uint8_t sparking = map(config.speed, 1, 255, 50, 180);

    for (int i = 0; i < numLeds; i++) {
        uint8_t cooldown = random(0, ((cooling * 10) / numLeds) + 2);
        if (cooldown > heatBuffer[i]) {
            heatBuffer[i] = 0;
        } else {
            heatBuffer[i] -= cooldown;
        }
    }

    for (int k = numLeds - 1; k >= 2; k--) {
        heatBuffer[k] = (heatBuffer[k - 1] + heatBuffer[k - 2] + heatBuffer[k - 2]) / 3;
    }

    if (random(255) < sparking) {
        int y = random(numLeds > 7 ? 7 : numLeds);
        heatBuffer[y] = heatBuffer[y] + random(160, 255);
        if (heatBuffer[y] > 255) heatBuffer[y] = 255;
    }

    for (int j = 0; j < numLeds; j++) {
        uint8_t t192 = (uint8_t)(((uint16_t)heatBuffer[j] * 191) >> 8);
        uint8_t heatramp = t192 & 0x3F;
        heatramp <<= 2;

        RgbColor color;
        if (t192 & 0x80) {
            color = RgbColor(255, 255, heatramp);
        } else if (t192 & 0x40) {
            color = RgbColor(255, heatramp, 0);
        } else {
            color = RgbColor(heatramp, 0, 0);
        }

        strip->SetPixelColor(j, scaleColor(color, config.brightness));
    }
}

void EffectsEngine::renderTwinkle(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    if (!twinkleBuffer) return;

    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);
    uint8_t spawnChance = map(config.intensity, 1, 255, 5, 40);

    for (int i = 0; i < numLeds; i++) {
        if (twinkleBuffer[i] > 0) {
            twinkleBuffer[i] = (twinkleBuffer[i] > 15) ? (twinkleBuffer[i] - 15) : 0;
        } else if (random(255) < spawnChance) {
            twinkleBuffer[i] = 255;
        }

        if (twinkleBuffer[i] > 0) {
            strip->SetPixelColor(i, scaleColor(scaled, twinkleBuffer[i]));
        } else {
            strip->SetPixelColor(i, scaleColor(scaled, 15));
        }
    }
}

void EffectsEngine::renderTheaterChase(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);
    uint8_t q = stepCounter % 3;

    for (uint16_t i = 0; i < numLeds; i++) {
        if ((i % 3) == q) {
            strip->SetPixelColor(i, scaled);
        } else {
            strip->SetPixelColor(i, RgbColor(0, 0, 0));
        }
    }
}

void EffectsEngine::renderGradient(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    uint8_t step = stepCounter & 0xFF;
    for (uint16_t i = 0; i < numLeds; i++) {
        uint8_t blend = sin8((i * 256 / numLeds) + step);
        RgbColor c1(config.r, config.g, config.b);
        RgbColor c2(config.b, config.r, config.g);

        uint8_t r = ((uint16_t)c1.R * (255 - blend) + (uint16_t)c2.R * blend) >> 8;
        uint8_t g = ((uint16_t)c1.G * (255 - blend) + (uint16_t)c2.G * blend) >> 8;
        uint8_t b = ((uint16_t)c1.B * (255 - blend) + (uint16_t)c2.B * blend) >> 8;

        strip->SetPixelColor(i, scaleColor(RgbColor(r, g, b), config.brightness));
    }
}

void EffectsEngine::renderPolice(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    uint8_t phase = (stepCounter >> 2) % 6;
    uint16_t half = numLeds / 2;
    RgbColor red(255, 0, 0);
    RgbColor blue(0, 0, 255);

    RgbColor sRed = scaleColor(red, config.brightness);
    RgbColor sBlue = scaleColor(blue, config.brightness);
    RgbColor off(0, 0, 0);

    for (uint16_t i = 0; i < numLeds; i++) {
        if (i < half) {
            strip->SetPixelColor(i, (phase == 0 || phase == 2) ? sRed : off);
        } else {
            strip->SetPixelColor(i, (phase == 3 || phase == 5) ? sBlue : off);
        }
    }
}

void EffectsEngine::renderAurora(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    float time = (float)stepCounter * 0.03f;
    for (uint16_t i = 0; i < numLeds; i++) {
        float x = (float)i / (float)numLeds * 3.0f;
        float w1 = sin(x * 2.0f + time);
        float w2 = sin(x * 4.0f - time * 0.7f);
        float w3 = sin(x * 1.5f + time * 1.3f);

        float val = (w1 + w2 + w3) / 3.0f;
        uint8_t norm = (uint8_t)((val + 1.0f) * 127.5f);

        RgbColor teal(0, 240, 255);
        RgbColor green(46, 204, 113);
        RgbColor violet(155, 89, 182);

        RgbColor col;
        if (norm < 128) {
            uint8_t b = norm * 2;
            col = RgbColor(
                ((uint16_t)teal.R * (255 - b) + (uint16_t)green.R * b) >> 8,
                ((uint16_t)teal.G * (255 - b) + (uint16_t)green.R * b) >> 8,
                ((uint16_t)teal.B * (255 - b) + (uint16_t)green.B * b) >> 8
            );
        } else {
            uint8_t b = (norm - 128) * 2;
            col = RgbColor(
                ((uint16_t)green.R * (255 - b) + (uint16_t)violet.R * b) >> 8,
                ((uint16_t)green.G * (255 - b) + (uint16_t)violet.G * b) >> 8,
                ((uint16_t)green.B * (255 - b) + (uint16_t)violet.B * b) >> 8
            );
        }

        strip->SetPixelColor(i, scaleColor(col, config.brightness));
    }
}

void EffectsEngine::renderMeteor(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    RgbColor base(config.r, config.g, config.b);
    RgbColor scaled = scaleColor(base, config.brightness);

    uint8_t meteorSize = map(config.intensity, 1, 255, 4, 15);
    uint8_t decay = 64;

    for (uint16_t i = 0; i < numLeds; i++) {
        RgbColor c = strip->GetPixelColor(i);
        if (c.R > 0 || c.G > 0 || c.B > 0) {
            strip->SetPixelColor(i, scaleColor(c, 255 - decay));
        }
    }

    uint16_t pos = stepCounter % (numLeds + meteorSize);
    for (uint8_t j = 0; j < meteorSize; j++) {
        int idx = pos - j;
        if (idx >= 0 && idx < numLeds) {
            strip->SetPixelColor(idx, scaled);
        }
    }
}

void EffectsEngine::renderRandomColors(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, const RgbConfig& config) {
    uint8_t step = (stepCounter >> 1) & 0xFF;
    for (uint16_t i = 0; i < numLeds; i++) {
        uint8_t pos = (step + (i * 17)) & 0xFF;
        strip->SetPixelColor(i, scaleColor(wheel(pos), config.brightness));
    }
}

void EffectsEngine::applyPowerLimiter(NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod>* strip, uint16_t maxCurrent_mA) {
    if (maxCurrent_mA == 0 || numLeds == 0) return;

    uint32_t totalCurrent_mA = numLeds * 1;

    for (uint16_t i = 0; i < numLeds; i++) {
        RgbColor c = strip->GetPixelColor(i);
        totalCurrent_mA += (c.R * 18 + c.G * 18 + c.B * 18) / 255;
    }

    if (totalCurrent_mA > maxCurrent_mA) {
        float scaleFactor = (float)maxCurrent_mA / (float)totalCurrent_mA;
        uint8_t scaleByte = (uint8_t)(scaleFactor * 255.0f);

        for (uint16_t i = 0; i < numLeds; i++) {
            RgbColor c = strip->GetPixelColor(i);
            strip->SetPixelColor(i, scaleColor(c, scaleByte));
        }
    }
}
