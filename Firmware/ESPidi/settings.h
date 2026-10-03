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
    uint8_t ptrnSwitch = 1;  // смена PTRN во время игры: 0=NOW, 1=NEXT, 2=END (см. seq_mel.h)
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