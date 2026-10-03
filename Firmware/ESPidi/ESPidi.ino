#include "hardware.h"
#include "app.h"
#include "midi_handler.h"
#include "inputs.h"
#include "ui.h"
#include "clock_engine.h"
#include "seq_mel.h"
#include "seq_song.h"
#include "engine.h"

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

    engine_begin();  // с этого момента MIDI и такты обслуживает задача движка
}

void loop() {
    // MIDI-вход, такты и MIDI-выход обслуживает задача движка (engine.cpp) каждые 0,5 мс.
    // Здесь — кнопки, энкодер, экран и сохранение; всё, что трогает состояние приложений,
    // выполняется под блокировкой движка.
    {
        EngineLock lock;
        inputs_pollEncoderFast();
        inputs_pollButtons();

        if (encDelta != 0) {
            int d = encDelta;
            encDelta -= d;  // ISR мог добавить шаг, пока обрабатываем
            ui_handleEncoder(d);
        }

        arp.update();
        melSeq.update();
        monitor.update();
        checkGlobalSave();
    }
    songSeq.update();               // подгрузка следующего паттерна песни: файл читается вне блокировки
    melSeq.loadRequestedPattern();  // смена PTRN из меню — так же

    {
        EngineLock lock;
        ui_drawScreen();  // рисуем кадр в буфер
    }
    ui_present();         // и отправляем на дисплей, не задерживая такты
    delay(1);             // отдать процессор задачам с низшим приоритетом (USB, сторожевой таймер)
}
