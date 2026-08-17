#include "clock_engine.h"
#include "hardware.h"
#include "app.h"
#include "arp.h"
#include "seq_mel.h"
#include "seq_song.h"

extern App* currentApp;
extern Arpeggiator arp;
extern MelodicSequencer melSeq;
extern SongSequencer songSeq;
void ui_markDirty(uint8_t flags);

uint8_t globalBpm = 120;

static bool sourceExternal = false;
static bool outEnabled = false;
static bool transportEnabled = false;

static bool clockAlive = false;
static bool bpmNeedFirstShow = true;
static float bpmSmooth = 120.0f;
static uint8_t displayBpm = 120;

static unsigned long lastClockInUs = 0;
static uint16_t windowTicks = 0;
static unsigned long windowStartUs = 0;

static unsigned long nextClockOutUs = 0;
static bool clockOutPhaseInit = false;
static uint32_t tickCount = 0;

static const unsigned long CLOCK_TIMEOUT_US = 500000;
static const unsigned long MEAS_WINDOW_US = 400000;

// 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
static const uint8_t DIV_TICKS[6] = { 96, 48, 24, 12, 6, 3 };

uint8_t clock_ticksPerDivision(uint8_t division) {
    if (division > 5) return 6;
    return DIV_TICKS[division];
}

uint32_t clock_getTickCount() {
    return tickCount;
}

void clock_dispatchTick() {
    tickCount++;
    if (arp.isEnabled()) arp.onClockTick();
    if (melSeq.isEnabled()) melSeq.onClockTick();
    if (songSeq.isEnabled()) songSeq.onClockTick();
}

void clock_begin() {
    clockAlive = false;
    bpmNeedFirstShow = true;
    bpmSmooth = (float)globalBpm;
    displayBpm = globalBpm;
    windowTicks = 0;
    windowStartUs = 0;
    lastClockInUs = 0;
    clockOutPhaseInit = false;
    tickCount = 0;
}

void clock_setSourceExternal(bool on) {
    sourceExternal = on;
    if (!on) {
        clockAlive = false;
        bpmNeedFirstShow = true;
        windowTicks = 0;
        windowStartUs = 0;
        lastClockInUs = 0;
    }
    clockOutPhaseInit = false;
}

void clock_setOutEnabled(bool on) {
    outEnabled = on;
    clockOutPhaseInit = false;
}

void clock_setTransportEnabled(bool on) {
    transportEnabled = on;
}

bool clock_isSourceExternal() { return sourceExternal; }
bool clock_isOutEnabled() { return outEnabled; }
bool clock_isTransportEnabled() { return transportEnabled; }
bool clock_isAlive() { return clockAlive; }

uint8_t clock_getBpm() { return globalBpm; }

void clock_setBpm(uint8_t bpm) {
    globalBpm = constrain(bpm, 40, 250);
    if (!sourceExternal || !clockAlive) {
        bpmSmooth = (float)globalBpm;
        displayBpm = globalBpm;
    }
    clockOutPhaseInit = false;
}

uint8_t clock_getDisplayBpm() {
    if (sourceExternal) {
        if (!clockAlive) return 0;
        return displayBpm;
    }
    return globalBpm;
}

static void resetMeasureWindow(unsigned long now) {
    windowTicks = 0;
    windowStartUs = now;
}

static void applyMeasuredBpm(float instant) {
    instant = constrain(instant, 30.0f, 300.0f);
    bpmSmooth = bpmSmooth * 0.85f + instant * 0.15f;

    uint8_t rounded = (uint8_t)(bpmSmooth + 0.5f);
    rounded = constrain(rounded, 30, 250);

    uint8_t playBpm = rounded < 40 ? 40 : rounded;
    if (playBpm != globalBpm) {
        globalBpm = playBpm;
        clockOutPhaseInit = false;
    }

    if (bpmNeedFirstShow || rounded != displayBpm) {
        int diff = (int)rounded - (int)displayBpm;
        if (diff < 0) diff = -diff;
        if (bpmNeedFirstShow || diff >= 2) {
            displayBpm = rounded;
            bpmNeedFirstShow = false;
            ui_markDirty(4);
        }
    }
}

void clock_onMidiClock() {
    if (!sourceExternal) return;

    unsigned long now = micros();

    if (outEnabled) {
        MIDI.sendRealTime(midi::Clock);
    }

    lastClockInUs = now;
    clockAlive = true;

    if (windowStartUs == 0) {
        resetMeasureWindow(now);
        windowTicks = 1;
    } else {
        windowTicks++;
        unsigned long elapsed = now - windowStartUs;
        if (elapsed >= MEAS_WINDOW_US && windowTicks >= 2) {
            float instant = (windowTicks * 60000000.0f) / ((float)elapsed * 24.0f);
            applyMeasuredBpm(instant);
            resetMeasureWindow(now);
        }
    }

    // Slave: каждый входящий Clock = один PPQN-тик для аппов
    clock_dispatchTick();
}

void clock_onMidiStart() {
    if (!sourceExternal) return;

    if (outEnabled) {
        MIDI.sendRealTime(midi::Start);
    }

    resetMeasureWindow(0);
    windowStartUs = 0;
    lastClockInUs = micros();
    clockAlive = true;
    tickCount = 0;

    if (transportEnabled && currentApp && !currentApp->isEnabled()) {
        currentApp->play();
    } else if (transportEnabled && currentApp) {
        currentApp->resetClockPhase();
    }
}

void clock_onMidiStop() {
    if (!sourceExternal) return;

    if (outEnabled) {
        MIDI.sendRealTime(midi::Stop);
    }

    clockAlive = false;
    bpmNeedFirstShow = true;
    windowTicks = 0;
    windowStartUs = 0;
    lastClockInUs = 0;
    ui_markDirty(4);

    if (transportEnabled && currentApp && currentApp->isEnabled()) {
        currentApp->stop();
    }
}

void clock_onMidiContinue() {
    if (!sourceExternal) return;

    if (outEnabled) {
        MIDI.sendRealTime(midi::Continue);
    }

    resetMeasureWindow(0);
    windowStartUs = 0;
    lastClockInUs = micros();
    clockAlive = true;

    if (transportEnabled && currentApp && !currentApp->isEnabled()) {
        currentApp->play();
    }
}

void clock_onLocalPlay(bool nowPlaying) {
    if (!transportEnabled) return;
    if (nowPlaying) {
        MIDI.sendRealTime(midi::Start);
    } else {
        MIDI.sendRealTime(midi::Stop);
    }
}

void clock_update() {
    unsigned long now = micros();

    if (sourceExternal && clockAlive) {
        if (lastClockInUs > 0 && (now - lastClockInUs) > CLOCK_TIMEOUT_US) {
            clockAlive = false;
            bpmNeedFirstShow = true;
            windowTicks = 0;
            windowStartUs = 0;
            ui_markDirty(4);
        }
    }

    // Slave — тики только с MIDI IN
    if (sourceExternal) return;

    bool needInternal =
        outEnabled ||
        arp.isEnabled() ||
        melSeq.isEnabled() ||
        songSeq.isEnabled();

    if (!needInternal) {
        clockOutPhaseInit = false;
        return;
    }

    uint8_t bpm = globalBpm;
    if (bpm < 40) bpm = 40;
    unsigned long interval = 60000000UL / ((unsigned long)bpm * 24UL);

    if (!clockOutPhaseInit) {
        nextClockOutUs = now;
        clockOutPhaseInit = true;
    }

    uint8_t burst = 0;
    while ((long)(now - nextClockOutUs) >= 0 && burst < 8) {
        clock_dispatchTick();
        if (outEnabled) {
            MIDI.sendRealTime(midi::Clock);
        }
        nextClockOutUs += interval;
        burst++;
        now = micros();
    }
    if ((long)(now - nextClockOutUs) > (long)(interval * 8)) {
        nextClockOutUs = now + interval;
    }
}
