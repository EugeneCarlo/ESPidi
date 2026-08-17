#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <Arduino.h>
#include <MIDI.h>
#include "hardware.h"
#include "app.h"
#include "seq_song.h"
#include "midi_monitor.h"
#include "settings.h"

#define SAVE_DELAY_MS 5000

extern App* currentApp;
extern SongSequencer songSeq;
extern MidiMonitor monitor;
extern SettingsApp settingsApp;

void midi_setup();
void midi_handleNoteOn(byte channel, byte note, byte velocity);
void midi_handleNoteOff(byte channel, byte note, byte velocity);
void midi_handleCC(byte channel, byte number, byte value);
void scheduleGlobalSave();
void checkGlobalSave();
void loadAllSettings();
void midi_handleClock();
void midi_handleStart();
void midi_handleStop();
void midi_handleContinue();

#endif