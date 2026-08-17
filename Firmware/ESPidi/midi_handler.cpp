#include "midi_handler.h"
#include "arp.h"
#include "seq_mel.h"
#include "seq_song.h"
#include "midi_monitor.h"
#include "settings.h"
#include "clock_engine.h"
#include <EEPROM.h>
#include "ui.h"

Arpeggiator arp;
MelodicSequencer melSeq;
SongSequencer songSeq;
MidiMonitor monitor;
SettingsApp settingsApp;
App* currentApp = &arp;
unsigned long lastChangeTime = 0;
bool needsSave = false;

void scheduleGlobalSave() {
    lastChangeTime = millis();
    needsSave = true;
}

static void syncAppsBpmFromGlobal() {
    arp.params.bpm = globalBpm;
    melSeq.params.bpm = globalBpm;
    songSeq.params.bpm = globalBpm;
    settingsApp.params.bpm = globalBpm;
}

void saveAllSettings() {
    syncAppsBpmFromGlobal();
    uint8_t appByte = (uint8_t)currentAppType;
    EEPROM.put(0, appByte);
    EEPROM.put(4, arp.params);
    EEPROM.put(4 + sizeof(ArpParams), melSeq.params);
    EEPROM.put(EEPROM_SEQ_DATA, melSeq.getCurrentPattern());
    EEPROM.put(EEPROM_SONG_PARAMS, songSeq.params);
    EEPROM.put(EEPROM_SONG_DATA, songSeq.getCurrentSong());
    EEPROM.put(EEPROM_SETTINGS, settingsApp.params);
    EEPROM.commit();
}

void loadAllSettings() {
    EEPROM.begin(512);
    
    uint8_t appByte;
    EEPROM.get(0, appByte);
    if (appByte < APP_COUNT) {
        currentAppType = (AppType)appByte;
    }
    
    ArpParams arpParams;
    EEPROM.get(4, arpParams);
    if (arpParams.bpm >= 40 && arpParams.bpm <= 250 &&
        arpParams.channel >= 1 && arpParams.channel <= 16 &&
        arpParams.mode <= 3 &&
        arpParams.octaves >= 1 && arpParams.octaves <= 4 &&
        arpParams.division <= 5 &&
        arpParams.gate <= 127) {
        arp.params = arpParams;
    }
    
    MelSeqParams seqParams;
    EEPROM.get(4 + sizeof(ArpParams), seqParams);
    if (seqParams.bpm >= 40 && seqParams.bpm <= 250 &&
        seqParams.channel >= 1 && seqParams.channel <= 16 &&
        seqParams.mode <= 3 &&
        seqParams.length >= 1 && seqParams.length <= 64 &&
        seqParams.gate <= 127 &&
        seqParams.swing <= 127 &&
        seqParams.randomness <= 127 &&
        seqParams.probability <= 127 &&
        seqParams.page >= 1 && seqParams.page <= 4 &&
        seqParams.strum <= 1 &&
        seqParams.reRec <= 1) {
        melSeq.params = seqParams;
    }
    
    uint8_t lastPattern = 0;
    EEPROM.get(EEPROM_SEQ_DATA, lastPattern);
    if (lastPattern < MAX_PATTERNS) {
        melSeq.loadFromFile(lastPattern);
    }
    
    SongParams songParams;
    EEPROM.get(EEPROM_SONG_PARAMS, songParams);
    if (songParams.bpm >= 40 && songParams.bpm <= 250 &&
        songParams.channel >= 1 && songParams.channel <= 16 &&
        songParams.mode <= 3 &&
        songParams.length >= 1 && songParams.length <= 64 &&
        songParams.cycle <= 1) {
        songSeq.params = songParams;
    }
    
    uint8_t lastSong = 0;
    EEPROM.get(EEPROM_SONG_DATA, lastSong);
    if (lastSong < MAX_SONGS) {
        songSeq.loadFromFile(lastSong);
    }

    SettingsParams settingsParams;
    EEPROM.get(EEPROM_SETTINGS, settingsParams);
    if (settingsParams.clockIn <= 1 &&
        settingsParams.clockOut <= 1 &&
        settingsParams.start <= 1 &&
        settingsParams.brightness >= 1 && settingsParams.brightness <= 8) {
        settingsApp.params = settingsParams;
        clock_setSourceExternal(settingsApp.params.clockIn == 1);
        clock_setOutEnabled(settingsApp.params.clockOut == 1);
        clock_setTransportEnabled(settingsApp.params.start == 1);

        if (settingsParams.bpm >= 40 && settingsParams.bpm <= 250) {
            clock_setBpm(settingsParams.bpm);
        } else if (arp.params.bpm >= 40 && arp.params.bpm <= 250) {
            clock_setBpm(arp.params.bpm);
        } else {
            clock_setBpm(120);
        }
    } else {
        clock_setBpm(arp.params.bpm >= 40 ? arp.params.bpm : 120);
    }

    syncAppsBpmFromGlobal();
    settingsApp.applyBrightness();
}

void checkGlobalSave() {
    if (needsSave && (millis() - lastChangeTime > SAVE_DELAY_MS)) {
        saveAllSettings();
        needsSave = false;
    }
}

void midi_setup() {
    MIDI.setHandleNoteOn(midi_handleNoteOn);
    MIDI.setHandleNoteOff(midi_handleNoteOff);
    MIDI.setHandleControlChange(midi_handleCC);
    MIDI.setHandleClock(midi_handleClock);
    MIDI.setHandleStart(midi_handleStart);
    MIDI.setHandleStop(midi_handleStop);
    MIDI.setHandleContinue(midi_handleContinue);
}

void midi_handleNoteOn(byte channel, byte note, byte velocity) {
    monitor.lastChannel = channel;
    monitor.handleNoteOn(note, velocity);
    
    if (currentAppType == APP_MONITOR) return;
    
    if (currentApp->handleNoteOn(note, velocity)) return;
    MIDI.sendNoteOn(note, velocity, channel);
}

void midi_handleNoteOff(byte channel, byte note, byte velocity) {
    monitor.lastChannel = channel;
    monitor.handleNoteOff(note);
    
    if (currentAppType == APP_MONITOR) return;
    
    if (currentApp->handleNoteOff(note)) return;
    MIDI.sendNoteOff(note, velocity, channel);
}

void midi_handleCC(byte channel, byte number, byte value) {
    monitor.handleCC(number, value);
    
    if (currentAppType == APP_MONITOR) return;
    
    if (currentApp->handleCC(number, value)) return;
    MIDI.sendControlChange(number, value, channel);
}

void midi_handleClock() {
    clock_onMidiClock();
}

void midi_handleStart() {
    clock_onMidiStart();
}

void midi_handleStop() {
    clock_onMidiStop();
}

void midi_handleContinue() {
    clock_onMidiContinue();
}
