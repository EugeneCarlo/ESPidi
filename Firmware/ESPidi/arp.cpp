#include "arp.h"
#include <MIDI.h>
#include "hardware.h"
#include "clock_engine.h"

void Arpeggiator::begin() {
}

void Arpeggiator::update() {
    // Timing — только через onClockTick
}

void Arpeggiator::resetClockPhase() {
    ticksIntoStep = 0;
    gateTicksLeft = 0;
}

uint16_t Arpeggiator::stepTicks() const {
    uint16_t base = clock_ticksPerDivision(params.division);
    if (params.swing == 0 || base < 2) return base;

    uint16_t swingAmt = (uint16_t)(((uint32_t)base * params.swing * 50) / 12700);
    if (swingAmt >= base) swingAmt = base - 1;

    if (currentStep % 2 == 1) return base + swingAmt;
    uint16_t t = base - swingAmt;
    return t < 1 ? 1 : t;
}

void Arpeggiator::advanceStep() {
    if (lastNote != 255) {
        MIDI.sendNoteOff(lastNote, 0, params.channel);
        lastNote = 255;
        playingNote = 255;
    }

    if (arpCount == 0) return;

    int idx = currentStep;
    if (idx >= 0 && idx < arpCount) {
        MIDI.sendNoteOn(arpNotes[idx], arpVel[idx], params.channel);
        lastNote = arpNotes[idx];
        playingNote = arpNotes[idx] % 12;

        uint16_t st = stepTicks();
        gateTicksLeft = (uint16_t)(((uint32_t)st * params.gate) / 127);
        if (gateTicksLeft >= st && st > 0) gateTicksLeft = st - 1;
        if (gateTicksLeft == 0 && params.gate > 0) gateTicksLeft = 1;
    }

    getNextStep();

    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(2);
}

void Arpeggiator::onClockTick() {
    if (!params.enabled) return;

    uint8_t activeCount = params.hold ? holdNoteCount : heldCount;
    if (activeCount == 0) return;

    if (arpCount == 0) buildNotes();
    if (arpCount == 0) return;

    if (gateTicksLeft > 0) {
        gateTicksLeft--;
        if (gateTicksLeft == 0 && lastNote != 255) {
            MIDI.sendNoteOff(lastNote, 0, params.channel);
            lastNote = 255;
            playingNote = 255;
            extern void ui_markDirty(uint8_t flags);
            ui_markDirty(2);
        }
    }

    ticksIntoStep++;
    uint16_t need = stepTicks();
    if (ticksIntoStep >= need) {
        ticksIntoStep = 0;
        advanceStep();
    }
}

bool Arpeggiator::handleNoteOn(uint8_t note, uint8_t velocity) {
    if (params.hold) {
        if (heldCount == 0) {
            holdNoteCount = 0;
        }
    }
    
    noteOn(note, velocity);
    
    if (params.hold) {
        holdNoteCount = heldCount;
        for (int i = 0; i < heldCount; i++) {
            holdNotes[i] = heldNotes[i];
            holdVelocities[i] = heldVelocities[i];
        }
    }
    
    if (params.enabled) {
        if (params.strum == 1) {
            if (strumCount < 16) {
                strumNotes[strumCount] = note;
                strumCount++;
            }
            MIDI.sendNoteOn(note, velocity, params.channel);
        }
        
        buildNotes();
        
        if (arpCount > 0) {
            currentStep = 0;
            ticksIntoStep = 0;
            MIDI.sendNoteOn(arpNotes[0], arpVel[0], params.channel);
            lastNote = arpNotes[0];
            playingNote = arpNotes[0] % 12;
            uint16_t st = stepTicks();
            gateTicksLeft = (uint16_t)(((uint32_t)st * params.gate) / 127);
            if (gateTicksLeft >= st && st > 0) gateTicksLeft = st - 1;
            getNextStep(); // следующий шаг по clock — уже 1
        }
        
        return true;
    }
    
    return false;
}

bool Arpeggiator::handleNoteOff(uint8_t note) {
    if (params.hold) {
        noteOff(note);
        return true;
    }
    
    noteOff(note);
    
    if (params.enabled) {
        if (heldCount == 0) {
            stopSounding();
        } else {
            buildNotes();
        }
        return true;
    }
    return false;
}

void Arpeggiator::play() {
    if (params.enabled) return;
    params.enabled = true;
    resetClockPhase();
    currentStep = 0;
    buildNotes();
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void Arpeggiator::stop() {
    if (!params.enabled) return;
    params.enabled = false;
    stopSounding();
    MIDI.sendControlChange(123, 0, params.channel);
    resetClockPhase();
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void Arpeggiator::toggle() {
    if (params.enabled) stop();
    else play();
}

void Arpeggiator::tap() {
    if (clock_isSourceExternal()) return;
    unsigned long now = millis();
    if (lastTapTime > 0 && (now - lastTapTime) < TAP_TIMEOUT_MS) {
        uint16_t newBpm = 60000 / (now - lastTapTime);
        clock_setBpm(constrain(newBpm, 40, 250));
    }
    lastTapTime = now;
}

void Arpeggiator::noteOn(uint8_t note, uint8_t velocity) {
    if (heldCount < MAX_HELD_NOTES) {
        for (int i = 0; i < heldCount; i++) {
            if (heldNotes[i] == note) {
                heldVelocities[i] = velocity;
                return;
            }
        }
        heldNotes[heldCount] = note;
        heldVelocities[heldCount] = velocity;
        heldCount++;
    }
}

void Arpeggiator::noteOff(uint8_t note) {
    for (int i = 0; i < heldCount; i++) {
        if (heldNotes[i] == note) {
            for (int j = i; j < heldCount - 1; j++) {
                heldNotes[j] = heldNotes[j + 1];
                heldVelocities[j] = heldVelocities[j + 1];
            }
            heldCount--;
            break;
        }
    }
}

void Arpeggiator::buildNotes() {
    arpCount = 0;
    
    uint8_t* sourceNotes = params.hold ? holdNotes : heldNotes;
    uint8_t* sourceVel = params.hold ? holdVelocities : heldVelocities;
    uint8_t sourceCount = params.hold ? holdNoteCount : heldCount;
    
    if (sourceCount == 0) return;
    
    for (int oct = 0; oct < params.octaves; oct++) {
        for (int i = 0; i < sourceCount; i++) {
            if (arpCount < 32) {
                arpNotes[arpCount] = sourceNotes[i] + (oct * 12);
                arpVel[arpCount] = sourceVel[i];
                arpCount++;
            }
        }
    }
}

int Arpeggiator::getNextStep() {
    if (arpCount == 0) return -1;
    
    switch (params.mode) {
        case 0: currentStep = (currentStep + 1) % arpCount; break;
        case 1: currentStep = (currentStep - 1 + arpCount) % arpCount; break;
        case 2:
            currentStep += direction;
            if (currentStep >= arpCount) { currentStep = arpCount - 2; direction = -1; }
            if (currentStep < 0) { currentStep = 1; direction = 1; }
            break;
        case 3: currentStep = random(arpCount); break;
    }
    return currentStep;
}

void Arpeggiator::stopStrum() {
    for (int i = 0; i < strumCount; i++) {
        MIDI.sendNoteOff(strumNotes[i], 0, params.channel);
    }
    strumCount = 0;
}

void Arpeggiator::stopSounding() {
    stopStrum();
    for (int i = 0; i < 128; i++) {
        MIDI.sendNoteOff(i, 0, params.channel);
    }
    MIDI.sendControlChange(123, 0, params.channel);
    playingNote = 255;
    lastNote = 255;
    gateTicksLeft = 0;
}

void Arpeggiator::enableHold() {
    if (heldCount > 0) {
        holdNoteCount = heldCount;
        for (int i = 0; i < heldCount; i++) {
            holdNotes[i] = heldNotes[i];
            holdVelocities[i] = heldVelocities[i];
        }
    }
}

void Arpeggiator::clearHold() {
    holdNoteCount = 0;
}
