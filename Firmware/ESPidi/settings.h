#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "config.h"
#include "app.h"

struct SettingsParams {
    uint8_t clockIn = 0;
    uint8_t clockOut = 0;
    uint8_t brightness = 4;
    uint8_t start = 0;  // 0=OFF, 1=ON — транспорт Start/Stop
    uint8_t bpm = 120;  // глобальный BPM (runtime + EEPROM)
};

class SettingsApp : public App {
public:
    SettingsParams params;
    
    void begin() override;
    void update() override;
    
    bool isEnabled() override { return true; }
    
    void applyBrightness();
};

#endif