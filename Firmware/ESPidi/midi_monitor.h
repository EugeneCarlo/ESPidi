// midi_monitor.h
#ifndef MIDI_MONITOR_H
#define MIDI_MONITOR_H

#include <Arduino.h>
#include "config.h"
#include "app.h"

#define MONITOR_MAX_NOTES 8
#define MONITOR_MAX_CC 6
#define MONITOR_NOTE_TIMEOUT_MS 10
#define MONITOR_CC_TIMEOUT_MS 1000

struct NoteEvent {
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    bool active;
    unsigned long timestamp;
    unsigned long noteOffTime;
};

struct CCEvent {
    uint8_t number;
    uint8_t value;
    unsigned long timestamp;
    bool dirty;
};

class MidiMonitor : public App {
public:
    void begin() override;
    void update() override;
    
    bool handleNoteOn(uint8_t note, uint8_t velocity) override;
    bool handleNoteOff(uint8_t note) override;
    bool handleCC(uint8_t number, uint8_t value) override;
    bool isEnabled() override { return true; }
    
    void toggleNoteDisplay();
    bool showNoteNames() const { return displayNoteNames; }
    
    uint8_t lastChannel;
    
    const NoteEvent* getActiveNotes() const { return notes; }
    uint8_t getActiveNoteCount() const { return activeNoteCount; }
    const CCEvent* getActiveCCs() const { return ccs; }
    uint8_t getActiveCCCount() const { return activeCCCount; }
    
private:
    NoteEvent notes[MONITOR_MAX_NOTES];
    CCEvent ccs[MONITOR_MAX_CC];
    
    uint8_t activeNoteCount = 0;
    uint8_t activeCCCount = 0;
    bool displayNoteNames = true;
    
    void addNote(uint8_t channel, uint8_t note, uint8_t velocity);
    void removeNote(uint8_t note);
    void addOrUpdateCC(uint8_t number, uint8_t value);
    void cleanupExpired();
    void sortNotes();
    void sortCCs();
};

#endif