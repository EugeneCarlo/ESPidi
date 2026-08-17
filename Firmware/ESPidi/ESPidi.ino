#include "hardware.h"
#include "app.h"
#include "midi_handler.h"
#include "inputs.h"
#include "ui.h"
#include "clock_engine.h"
#include "seq_mel.h"
#include "seq_song.h"

extern Arpeggiator arp;
extern MelodicSequencer melSeq;
extern SongSequencer songSeq;

void setup() {
    hw_initPins();
    hw_initDisplay();
    hw_initMIDI();
    inputs_init();
    clock_begin();
    midi_setup();
    
    arp.begin();
    melSeq.begin();
    songSeq.begin();
    loadAllSettings();
    settingsApp.begin();

    ui_setApp(currentAppType);
    ui_markDirty(UI_DIRTY_FULL);
}

void loop() {
    // MIDI и clock — высший приоритет
    MIDI.read();
    clock_update();
    inputs_pollEncoderFast();

    MIDI.read();
    inputs_pollButtons();

    if (encDelta != 0) {
        ui_handleEncoder(encDelta);
        encDelta = 0;
    }

    MIDI.read();

    arp.update();
    melSeq.update();
    songSeq.update();
    monitor.update();
    // PPQN-тики аппам: clock_update() → clock_dispatchTick()

    MIDI.read();
    clock_update();

    checkGlobalSave();
    ui_drawScreen();
}
