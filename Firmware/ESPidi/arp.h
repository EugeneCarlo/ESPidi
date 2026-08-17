#ifndef ARP_H
#define ARP_H

#include <Arduino.h>
#include "config.h"
#include "app.h"

struct ArpParams {
    uint8_t bpm = 120;
    uint8_t channel = 1;
    uint8_t mode = 0;
    uint8_t octaves = 1;
    uint8_t division = 4;
    uint8_t gate = 80;
    bool enabled = false;
    uint8_t hold = 0;
    uint8_t strum = 1;
    uint8_t swing = 0;
    uint8_t reserved_slide = 0;
};

class Arpeggiator : public App {
public:
    ArpParams params;
    
    void begin() override;
    void update() override;
    void onClockTick() override;
    void resetClockPhase() override;
    
    bool handleNoteOn(uint8_t note, uint8_t velocity) override;
    bool handleNoteOff(uint8_t note) override;
    bool isEnabled() override { return params.enabled; }
    
    void play() override;
    void stop() override;
    void toggle() override;
    void tap() override;
    
    void rebuildNotes() { buildNotes(); }
    void enableHold();
    void clearHold();
    
    uint8_t getHeldCount() const { return heldCount; }
    const uint8_t* getHeldNotes() const { return heldNotes; }
    uint8_t getPlayingNote() const { return playingNote; }
    uint8_t getHoldNoteCount() const { return holdNoteCount; }
    const uint8_t* getHoldNotes() const { return holdNotes; }
    
private:
    uint8_t heldNotes[MAX_HELD_NOTES];
    uint8_t heldVelocities[MAX_HELD_NOTES];
    uint8_t heldCount = 0;
    
    uint8_t holdNotes[MAX_HELD_NOTES];
    uint8_t holdVelocities[MAX_HELD_NOTES];
    uint8_t holdNoteCount = 0;
    
    uint8_t arpNotes[32];
    uint8_t arpVel[32];
    uint8_t arpCount = 0;
    
    uint8_t currentStep = 0;
    int8_t direction = 1;
    uint8_t playingNote = 255;
    uint8_t lastNote = 255;
    unsigned long lastTapTime = 0;
    
    uint16_t ticksIntoStep = 0;
    uint16_t gateTicksLeft = 0;
    
    uint8_t strumNotes[16];
    uint8_t strumCount = 0;
    
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void buildNotes();
    int getNextStep();
    void stopSounding();
    void stopStrum();
    uint16_t stepTicks() const;
    void advanceStep();
};

#endif
