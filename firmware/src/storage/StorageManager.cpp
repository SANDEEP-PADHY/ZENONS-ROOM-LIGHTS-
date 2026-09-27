#include "StorageManager.h"

StorageManager::StorageManager() {}

bool StorageManager::begin() {
    return prefs.begin("roomlights", false);
}

void StorageManager::loadRelays(RelayConfig relays[MAX_RELAY_CHANNELS]) {
    // Copy defaults first
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        relays[i] = DEFAULT_RELAYS[i];
    }

    if (!prefs.getBool("init_relays", false)) {
        // Save initial defaults to NVS
        saveAllRelays(relays);
        prefs.putBool("init_relays", true);
        return;
    }

    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        String prefix = "r" + String(i + 1) + "_";
        String name = prefs.getString((prefix + "name").c_str(), DEFAULT_RELAYS[i].name);
        strncpy(relays[i].name, name.c_str(), sizeof(relays[i].name) - 1);
        relays[i].gpio = prefs.getChar((prefix + "gpio").c_str(), DEFAULT_RELAYS[i].gpio);
        relays[i].activeLow = prefs.getBool((prefix + "act").c_str(), DEFAULT_RELAYS[i].activeLow);
        relays[i].enabled = prefs.getBool((prefix + "en").c_str(), DEFAULT_RELAYS[i].enabled);
        relays[i].state = prefs.getBool((prefix + "st").c_str(), false);
        String type = prefs.getString((prefix + "type").c_str(), DEFAULT_RELAYS[i].type);
        strncpy(relays[i].type, type.c_str(), sizeof(relays[i].type) - 1);
    }
}

void StorageManager::saveRelay(const RelayConfig& relay) {
    if (relay.id < 1 || relay.id > MAX_RELAY_CHANNELS) return;
    String prefix = "r" + String(relay.id) + "_";
    prefs.putString((prefix + "name").c_str(), relay.name);
    prefs.putChar((prefix + "gpio").c_str(), relay.gpio);
    prefs.putBool((prefix + "act").c_str(), relay.activeLow);
    prefs.putBool((prefix + "en").c_str(), relay.enabled);
    prefs.putBool((prefix + "st").c_str(), relay.state);
    prefs.putString((prefix + "type").c_str(), relay.type);
}

void StorageManager::saveRelayState(uint8_t id, bool state) {
    if (id < 1 || id > MAX_RELAY_CHANNELS) return;
    String key = "r" + String(id) + "_st";
    prefs.putBool(key.c_str(), state);
}

void StorageManager::saveAllRelays(const RelayConfig relays[MAX_RELAY_CHANNELS]) {
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        saveRelay(relays[i]);
    }
}

void StorageManager::loadRgb(RgbConfig& rgb) {
    if (!prefs.getBool("init_rgb", false)) {
        rgb.power = false;
        rgb.brightness = DEFAULT_RGB_BRIGHTNESS;
        rgb.r = 0;
        rgb.g = 240;
        rgb.b = 255;
        strncpy(rgb.effect, "aurora", sizeof(rgb.effect) - 1);
        rgb.speed = 128;
        rgb.intensity = 160;
        strncpy(rgb.chipset, DEFAULT_RGB_CHIPSET, sizeof(rgb.chipset) - 1);
        rgb.ledCount = DEFAULT_LED_COUNT;
        rgb.gpio = DEFAULT_RGB_GPIO;
        strncpy(rgb.colorOrder, DEFAULT_COLOR_ORDER, sizeof(rgb.colorOrder) - 1);
        rgb.maxCurrent_mA = DEFAULT_MAX_CURRENT_MA;

        saveRgb(rgb);
        prefs.putBool("init_rgb", true);
        return;
    }

    rgb.power = prefs.getBool("rgb_pwr", false);
    rgb.brightness = prefs.getUChar("rgb_bri", DEFAULT_RGB_BRIGHTNESS);
    rgb.r = prefs.getUChar("rgb_r", 0);
    rgb.g = prefs.getUChar("rgb_g", 240);
    rgb.b = prefs.getUChar("rgb_b", 255);
    String eff = prefs.getString("rgb_eff", "aurora");
    strncpy(rgb.effect, eff.c_str(), sizeof(rgb.effect) - 1);
    rgb.speed = prefs.getUChar("rgb_spd", 128);
    rgb.intensity = prefs.getUChar("rgb_int", 160);

    String chip = prefs.getString("rgb_chip", DEFAULT_RGB_CHIPSET);
    strncpy(rgb.chipset, chip.c_str(), sizeof(rgb.chipset) - 1);
    rgb.ledCount = prefs.getUShort("rgb_cnt", DEFAULT_LED_COUNT);
    rgb.gpio = prefs.getChar("rgb_pin", DEFAULT_RGB_GPIO);
    String ord = prefs.getString("rgb_ord", DEFAULT_COLOR_ORDER);
    strncpy(rgb.colorOrder, ord.c_str(), sizeof(rgb.colorOrder) - 1);
    rgb.maxCurrent_mA = prefs.getUShort("rgb_ma", DEFAULT_MAX_CURRENT_MA);
}

void StorageManager::saveRgb(const RgbConfig& rgb) {
    prefs.putBool("rgb_pwr", rgb.power);
    prefs.putUChar("rgb_bri", rgb.brightness);
    prefs.putUChar("rgb_r", rgb.r);
    prefs.putUChar("rgb_g", rgb.g);
    prefs.putUChar("rgb_b", rgb.b);
    prefs.putString("rgb_eff", rgb.effect);
    prefs.putUChar("rgb_spd", rgb.speed);
    prefs.putUChar("rgb_int", rgb.intensity);
    prefs.putString("rgb_chip", rgb.chipset);
    prefs.putUShort("rgb_cnt", rgb.ledCount);
    prefs.putChar("rgb_pin", rgb.gpio);
    prefs.putString("rgb_ord", rgb.colorOrder);
    prefs.putUShort("rgb_ma", rgb.maxCurrent_mA);
}

void StorageManager::saveRgbState(bool power, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b, const char* effect, uint8_t speed, uint8_t intensity) {
    prefs.putBool("rgb_pwr", power);
    prefs.putUChar("rgb_bri", brightness);
    prefs.putUChar("rgb_r", r);
    prefs.putUChar("rgb_g", g);
    prefs.putUChar("rgb_b", b);
    prefs.putString("rgb_eff", effect);
    prefs.putUChar("rgb_spd", speed);
    prefs.putUChar("rgb_int", intensity);
}

void StorageManager::loadNetworkConfig(String& ssid, String& pass, String& hostname, bool& useStatic, String& ip, String& gw, String& subnet, String& dns) {
    if (!prefs.getBool("init_net", false)) {
        ssid = DEFAULT_WIFI_SSID;
        pass = DEFAULT_WIFI_PASS;
        hostname = DEFAULT_MDNS_HOSTNAME;
        useStatic = DEFAULT_USE_STATIC_IP;
        ip = DEFAULT_STATIC_IP;
        gw = DEFAULT_GATEWAY;
        subnet = DEFAULT_SUBNET;
        dns = DEFAULT_DNS;
        saveNetworkConfig(ssid, pass, hostname, useStatic, ip, gw, subnet, dns);
        prefs.putBool("init_net", true);
        return;
    }

    ssid = prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID);
    if (ssid == "sandeep" || ssid.isEmpty()) {
        ssid = "Sandeep";
        prefs.putString("wifi_ssid", ssid);
    }
    pass = prefs.getString("wifi_pass", DEFAULT_WIFI_PASS);
    
    hostname = "zenon";
    useStatic = false;
    ip = DEFAULT_STATIC_IP;
    gw = DEFAULT_GATEWAY;
    subnet = DEFAULT_SUBNET;
    dns = DEFAULT_DNS;
    
    // Save to persist
    saveNetworkConfig(ssid, pass, hostname, useStatic, ip, gw, subnet, dns);
}

void StorageManager::saveNetworkConfig(const String& ssid, const String& pass, const String& hostname, bool useStatic, const String& ip, const String& gw, const String& subnet, const String& dns) {
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.putString("net_host", hostname);
    prefs.putBool("net_static", useStatic);
    prefs.putString("net_ip", ip);
    prefs.putString("net_gw", gw);
    prefs.putString("net_sub", subnet);
    prefs.putString("net_dns", dns);
}

void StorageManager::factoryReset() {
    prefs.clear();
}
