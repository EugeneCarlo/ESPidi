#ifndef APP_H
#define APP_H

#include <Arduino.h>

enum AppType {
    APP_ARPEGGIATOR = 0,
    APP_MEL_SEQ = 1,
    APP_SONG = 2,
    APP_MONITOR = 3,
    APP_SETTINGS = 4,
    APP_COUNT = 5
};

inline const char* appNames[] = {"ARP", "SEQUENCER", "SONG", "MONITOR", "SETTINGS"};

struct App {
    virtual void begin() {}
    virtual void update() {}
    virtual void onClockTick() {}
    virtual void resetClockPhase() {}
    virtual void play() {}
    virtual void stop() {}
    virtual void toggle() {
        if (isEnabled()) stop();
        else play();
    }
    virtual void tap() {}
    virtual bool handleNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) { (void)channel; return false; }
    virtual bool handleNoteOff(uint8_t note, uint8_t channel) { (void)channel; return false; }
    virtual bool handleCC(uint8_t number, uint8_t value, uint8_t channel) { (void)channel; return false; }
    virtual bool isEnabled() { return false; }
    // Проходят ли входящие сообщения на выход так же, как ноты (THRU). По этому же правилу
    // проходят Pitch Bend, Program Change, Aftertouch, SysEx и системные сообщения.
    virtual bool passesThru() { return true; }
};

#endif