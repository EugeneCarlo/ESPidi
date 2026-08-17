#include "settings.h"
#include "hardware.h"
#include "clock_engine.h"

void SettingsApp::begin() {
    applyBrightness();
}

void SettingsApp::update() {
}

void SettingsApp::applyBrightness() {
    const uint8_t brightLevels[] = {10, 45, 80, 115, 150, 185, 220, 255};
    uint8_t level = constrain(params.brightness, 1, 8);
    uint8_t realBright = brightLevels[level - 1];
    display.ssd1306_command(SSD1306_SETPRECHARGE);
    display.ssd1306_command((realBright < 128) ? 0x11 : 0xF1);
    display.ssd1306_command(SSD1306_SETVCOMDETECT);
    display.ssd1306_command(realBright / 4);
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(realBright);
}
