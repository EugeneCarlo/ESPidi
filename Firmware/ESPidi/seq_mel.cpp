#include "seq_mel.h"
#include <MIDI.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "hardware.h"
#include "midi_handler.h"
#include "clock_engine.h"

// === Active note pool (shared helpers) ===

void MelodicSequencer::activePoolTick(SeqActiveNote* pool, int maxPool) {
    for (int i = 0; i < maxPool; i++) {
        if (!pool[i].used) continue;
        if (pool[i].ticksLeft > 0) pool[i].ticksLeft--;
        if (pool[i].ticksLeft == 0) {
            MIDI.sendNoteOff(pool[i].note, 0, pool[i].channel);
            pool[i].used = false;
        }
    }
}

void MelodicSequencer::activePoolStopAll(SeqActiveNote* pool, int maxPool) {
    for (int i = 0; i < maxPool; i++) {
        if (!pool[i].used) continue;
        MIDI.sendNoteOff(pool[i].note, 0, pool[i].channel);
        pool[i].used = false;
    }
    for (uint8_t ch = 1; ch <= 16; ch++) {
        MIDI.sendControlChange(123, 0, ch);
    }
}

void MelodicSequencer::activePoolNoteOn(SeqActiveNote* pool, int maxPool,
    uint8_t note, uint8_t channel, uint8_t velocity, uint16_t ticks) {
    if (ticks < 1) ticks = 1;
    for (int i = 0; i < maxPool; i++) {
        if (pool[i].used && pool[i].note == note && pool[i].channel == channel) {
            pool[i].velocity = velocity;
            pool[i].ticksLeft = ticks;
            MIDI.sendNoteOn(note, velocity, channel);
            return;
        }
    }
    for (int i = 0; i < maxPool; i++) {
        if (!pool[i].used) {
            pool[i].note = note;
            pool[i].channel = channel;
            pool[i].velocity = velocity;
            pool[i].ticksLeft = ticks;
            pool[i].used = true;
            MIDI.sendNoteOn(note, velocity, channel);
            return;
        }
    }
}

uint16_t MelodicSequencer::computeNoteLengthTicks(
    uint8_t startStep, uint8_t noteIndex, uint8_t patternLength,
    uint16_t stepLenTicks, uint8_t gate,
    const bool* tieArr, const uint8_t* lenArr) {
    if (patternLength == 0) return stepLenTicks;
    uint16_t len = lenArr[startStep * MAX_POLY + noteIndex];
    if (len == 0) {
        len = (uint16_t)(((uint32_t)stepLenTicks * gate) / 127);
        if (len < 1) len = 1;
    }
    uint8_t s = startStep;
    uint8_t guard = 0;
    while (tieArr[s] && guard < patternLength) {
        s = (s + 1) % patternLength;
        len += stepLenTicks;
        guard++;
        if (s == startStep) break;
    }
    return len;
}

// === Lifecycle ===

void MelodicSequencer::begin() {
    initArrays();
    if (!LittleFS.begin(true)) {
    }
    editStep = 0;
}

void MelodicSequencer::initArrays() {
    currentPattern = 0;
    patternDirty = false;
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
        for (int j = 0; j < MAX_POLY; j++) {
            notes[i][j] = -1;
            velocities[i][j] = 100;
            channels[i][j] = 1;
            lengthTicks[i][j] = 0;
        }
        noteCount[i] = 0;
        for (int j = 0; j < MAX_CC_PER_STEP; j++) {
            ccNumber[i][j] = 0;
            ccValue[i][j] = 0;
        }
        ccCount[i] = 0;
        tie[i] = false;
        transpose[i] = 0;
    }
    activePoolStopAll(activeNotes, MAX_ACTIVE_SEQ_NOTES);
}

void MelodicSequencer::update() {
    if (!enabled) return;
    if (params.follow == 1) {
        uint8_t newPage = (currentStep / STEPS_PER_PAGE) + 1;
        if (newPage != params.page && newPage >= 1 && newPage <= 4) {
            params.page = newPage;
        }
    }
}

void MelodicSequencer::resetClockPhase() {
    ticksIntoStep = 0;
}

uint16_t MelodicSequencer::stepTicks() const {
    uint16_t base = clock_ticksPerDivision(4);
    if (params.swing == 0 || base < 2) return base;
    uint16_t swingAmt = (uint16_t)(((uint32_t)base * params.swing * 50) / 12700);
    if (swingAmt >= base) swingAmt = base - 1;
    if (currentStep % 2 == 1) return base + swingAmt;
    uint16_t t = base - swingAmt;
    return t < 1 ? 1 : t;
}

uint16_t MelodicSequencer::defaultGateTicks() const {
    uint16_t st = stepTicks();
    uint16_t len = (uint16_t)(((uint32_t)st * params.gate) / 127);
    return len < 1 ? 1 : len;
}

uint16_t MelodicSequencer::noteLengthFor(uint8_t step, uint8_t index) const {
    if (step >= MAX_SEQ_STEPS || index >= MAX_POLY) return defaultGateTicks();
    uint8_t flatLen[MAX_SEQ_STEPS * MAX_POLY];
    for (int s = 0; s < MAX_SEQ_STEPS; s++) {
        for (int n = 0; n < MAX_POLY; n++) {
            flatLen[s * MAX_POLY + n] = lengthTicks[s][n];
        }
    }
    return computeNoteLengthTicks(step, index, params.length, stepTicks(), params.gate, tie, flatLen);
}

void MelodicSequencer::extendActiveNotesThroughTie(uint8_t step) {
    if (step >= MAX_SEQ_STEPS || !tie[step]) return;
    uint16_t extra = stepTicks();
    for (int i = 0; i < MAX_ACTIVE_SEQ_NOTES; i++) {
        if (!activeNotes[i].used) continue;
        activeNotes[i].ticksLeft += extra;
    }
}

void MelodicSequencer::onClockTick() {
    if (!enabled) return;
    activePoolTick(activeNotes, MAX_ACTIVE_SEQ_NOTES);
    if (params.length == 0) return;

    ticksIntoStep++;
    if (ticksIntoStep >= stepTicks()) {
        ticksIntoStep = 0;
        doStep();
    }
}

// Куда писать ноту при записи: в STEP EDIT — в выбранный шаг; во время игры — в шаг,
// который сейчас звучит (currentStep к этому моменту уже указывает на следующий).
uint8_t MelodicSequencer::recordTargetStep() const {
    if (stepEditActive) return editStep;
    if (enabled) return lastPlayedStep;
    return currentStep;
}

void MelodicSequencer::doStep() {
    if (currentStep >= params.length) {
        currentStep %= params.length;  // LENGTH уменьшили на ходу — сразу возвращаемся в границы
    }

    bool doRandomize = false;
    if (params.probability > 0 && params.randomness > 0) {
        if (random(127) < params.probability) doRandomize = true;
    }

    if (recording && noteHeld && !stepEditActive && heldStep != currentStep) {
        // Нота держится через границу шага: связываем шаг, где она записана, со следующим.
        tie[heldStep] = true;
        heldStep = currentStep;
        patternDirty = true;
    }
    lastPlayedStep = currentStep;

    uint8_t prevStep = (currentStep == 0) ? params.length - 1 : currentStep - 1;

    if (noteCount[currentStep] == 0 && tie[currentStep]) {
        // Пустой связанный шаг: длина звучащих нот уже включает всю цепочку Tie
        // (computeNoteLengthTicks при старте ноты) — повторно не продлеваем.
    } else {
        if (!tie[prevStep]) {
            activePoolStopAll(activeNotes, MAX_ACTIVE_SEQ_NOTES);
        }
        playStep(currentStep, doRandomize);
    }

    bool randomJump = false;
    if (doRandomize) {
        uint8_t jumpChance = params.randomness / 5;
        if (jumpChance > 0 && random(127) < jumpChance) {
            currentStep = random(params.length);
            randomJump = true;
        }
    }

    if (!randomJump) {
        switch (params.mode) {
            case 0: currentStep = (currentStep + 1) % params.length; break;
            case 1: currentStep = (currentStep - 1 + params.length) % params.length; break;
            case 2:
                if (params.length <= 1) {
                    currentStep = 0;
                    break;
                }
                {
                    int nextStep = currentStep + direction;
                    if (nextStep >= params.length || nextStep < 0) {
                        direction = -direction;
                        nextStep = currentStep + direction;
                    }
                    currentStep = nextStep;
                }
                break;
            case 3: currentStep = random(params.length); break;
        }
    }

    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(2);
}

void MelodicSequencer::playStep(uint8_t step, bool doRandomize) {
    int8_t rndAmt = doRandomize ? (params.randomness / 2) : 0;

    for (int i = 0; i < ccCount[step]; i++) {
        uint8_t ccVal = ccValue[step][i];
        if (doRandomize) {
            int16_t delta = random(-rndAmt, rndAmt + 1);
            ccVal = constrain((int16_t)ccVal + delta, 0, 127);
        }
        MIDI.sendControlChange(ccNumber[step][i], ccVal, params.channel);
    }

    if (noteCount[step] == 0) return;

    if (doRandomize) {
        uint8_t muteChance = params.randomness / 4;
        if (muteChance > 0 && random(127) < muteChance) return;
    }

    uint8_t prevStep = (step == 0) ? params.length - 1 : step - 1;
    bool hasTieFromPrev = tie[prevStep] && noteCount[prevStep] > 0;

    for (int i = 0; i < noteCount[step]; i++) {
        if (notes[step][i] < 0 || notes[step][i] >= 128) continue;

        int16_t transposedNote = (int16_t)notes[step][i] + transpose[step];
        transposedNote = constrain(transposedNote, 0, 127);
        uint8_t ch = channels[step][i];
        if (ch < 1 || ch > 16) ch = 1;

        bool skipNoteOn = false;
        if (hasTieFromPrev) {
            for (int a = 0; a < MAX_ACTIVE_SEQ_NOTES; a++) {
                if (activeNotes[a].used &&
                    activeNotes[a].note == (uint8_t)transposedNote &&
                    activeNotes[a].channel == ch) {
                    skipNoteOn = true;
                    activeNotes[a].ticksLeft = noteLengthFor(step, i);
                    break;
                }
            }
        }

        if (!skipNoteOn) {
            uint8_t vel = velocities[step][i];
            if (doRandomize) {
                int16_t delta = random(-rndAmt, rndAmt + 1);
                vel = constrain((int16_t)vel + delta, 10, 127);
            }
            activePoolNoteOn(activeNotes, MAX_ACTIVE_SEQ_NOTES,
                (uint8_t)transposedNote, ch, vel, noteLengthFor(step, i));
        }
    }
}

void MelodicSequencer::stopAllNotes() {
    activePoolStopAll(activeNotes, MAX_ACTIVE_SEQ_NOTES);
}

// === MIDI in ===

// STRUM работает как THRU: ON — входящий сигнал проходит на выход (ровно один раз, через
// диспетчер), OFF — не проходит. Запись от STRUM не зависит. Возвращаемое значение
// «обработано» = true значит «на выход не передавать».
bool MelodicSequencer::handleNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (recording && channel == params.channel) recordNote(note, velocity, channel);
    return params.strum == 0;
}

bool MelodicSequencer::handleNoteOff(uint8_t note, uint8_t channel) {
    (void)note;
    (void)channel;
    if (recording) noteHeld = false;
    return params.strum == 0;
}

bool MelodicSequencer::handleCC(uint8_t number, uint8_t value, uint8_t channel) {
    if (recording && channel == params.channel) recordCC(number, value);
    return params.strum == 0;
}

void MelodicSequencer::recordNote(uint8_t note, uint8_t velocity, uint8_t channel) {
    uint8_t targetStep = recordTargetStep();
    if (targetStep >= MAX_SEQ_STEPS) return;

    unsigned long now = millis();
    bool newChord = (now - lastChordTime > 80);

    if (params.reRec == 1 && newChord && !noteHeld) {
        for (int j = 0; j < MAX_POLY; j++) {
            notes[targetStep][j] = -1;
            velocities[targetStep][j] = 100;
            channels[targetStep][j] = params.channel;
            lengthTicks[targetStep][j] = 0;
        }
        noteCount[targetStep] = 0;
        tie[targetStep] = false;
    }

    lastChordTime = now;

    for (int j = 0; j < noteCount[targetStep]; j++) {
        if (notes[targetStep][j] == (int8_t)note && channels[targetStep][j] == channel) {
            velocities[targetStep][j] = velocity;
            noteHeld = true;
            heldStep = targetStep;
            patternDirty = true;
            scheduleGlobalSave();
            return;
        }
    }

    if (noteCount[targetStep] >= MAX_POLY) {
        for (int j = 0; j < MAX_POLY - 1; j++) {
            notes[targetStep][j] = notes[targetStep][j + 1];
            velocities[targetStep][j] = velocities[targetStep][j + 1];
            channels[targetStep][j] = channels[targetStep][j + 1];
            lengthTicks[targetStep][j] = lengthTicks[targetStep][j + 1];
        }
        noteCount[targetStep] = MAX_POLY - 1;
    }

    notes[targetStep][noteCount[targetStep]] = note;
    velocities[targetStep][noteCount[targetStep]] = velocity;
    channels[targetStep][noteCount[targetStep]] = channel;
    lengthTicks[targetStep][noteCount[targetStep]] = 0;
    noteCount[targetStep]++;
    noteHeld = true;
    heldStep = targetStep;
    patternDirty = true;
    scheduleGlobalSave();

    return;
}

void MelodicSequencer::recordCC(uint8_t number, uint8_t value) {
    uint8_t targetStep = recordTargetStep();
    if (targetStep >= MAX_SEQ_STEPS) return;

    for (int i = 0; i < ccCount[targetStep]; i++) {
        if (ccNumber[targetStep][i] == number) {
            ccValue[targetStep][i] = value;
            patternDirty = true;
            scheduleGlobalSave();
            return;
        }
    }

    if (ccCount[targetStep] >= MAX_CC_PER_STEP) {
        for (int j = 0; j < MAX_CC_PER_STEP - 1; j++) {
            ccNumber[targetStep][j] = ccNumber[targetStep][j + 1];
            ccValue[targetStep][j] = ccValue[targetStep][j + 1];
        }
        ccCount[targetStep] = MAX_CC_PER_STEP - 1;
    }

    ccNumber[targetStep][ccCount[targetStep]] = number;
    ccValue[targetStep][ccCount[targetStep]] = value;
    ccCount[targetStep]++;
    patternDirty = true;
    scheduleGlobalSave();

    return;
}

// === Transport ===

void MelodicSequencer::play() {
    if (enabled) return;
    enabled = true;
    resetClockPhase();
    direction = 1;
    if (params.mode == 1) currentStep = params.length - 1;
    else currentStep = 0;
    editStep = currentStep;
    lastPlayedStep = currentStep;
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void MelodicSequencer::stop() {
    enabled = false;
    stopAllNotes();
    resetClockPhase();
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void MelodicSequencer::toggle() {
    if (enabled) stop();
    else play();
}

void MelodicSequencer::toggleRecord() {
    recording = !recording;
}

void MelodicSequencer::clear() {
    uint8_t keepPattern = currentPattern;  // очистка не меняет номер слота паттерна
    initArrays();
    currentPattern = keepPattern;
    patternDirty = true;
    scheduleGlobalSave();
}

void MelodicSequencer::clearStep(uint8_t step) {
    if (step >= MAX_SEQ_STEPS) return;
    for (int j = 0; j < MAX_POLY; j++) {
        notes[step][j] = -1;
        velocities[step][j] = 100;
        channels[step][j] = params.channel;
        lengthTicks[step][j] = 0;
    }
    noteCount[step] = 0;
    for (int j = 0; j < MAX_CC_PER_STEP; j++) {
        ccNumber[step][j] = 0;
        ccValue[step][j] = 0;
    }
    ccCount[step] = 0;
    tie[step] = false;
    transpose[step] = 0;
    patternDirty = true;
    scheduleGlobalSave();
}

void MelodicSequencer::toggleTie(uint8_t step) {
    if (step >= MAX_SEQ_STEPS) return;
    tie[step] = !tie[step];
    patternDirty = true;
    scheduleGlobalSave();
}

void MelodicSequencer::tap() {
    if (clock_isSourceExternal()) return;
    unsigned long now = millis();
    if (lastTapTime > 0 && (now - lastTapTime) < TAP_TIMEOUT_MS) {
        uint16_t newBpm = 60000 / (now - lastTapTime);
        clock_setBpm(constrain(newBpm, 40, 250));
    }
    lastTapTime = now;
}

// === UI accessors (filtered by params.channel) ===

uint8_t MelodicSequencer::getDisplayNoteCount(uint8_t step) const {
    if (step >= MAX_SEQ_STEPS) return 0;
    uint8_t count = 0;
    for (int i = 0; i < noteCount[step]; i++) {
        if (channels[step][i] == params.channel && notes[step][i] >= 0) count++;
    }
    return count;
}

bool MelodicSequencer::getHasNote(uint8_t step) const {
    return getDisplayNoteCount(step) > 0;
}

int MelodicSequencer::findStoredNoteIndex(uint8_t step, uint8_t displayIndex) const {
    if (step >= MAX_SEQ_STEPS) return -1;
    uint8_t found = 0;
    for (int i = 0; i < noteCount[step]; i++) {
        if (channels[step][i] == params.channel && notes[step][i] >= 0) {
            if (found == displayIndex) return i;
            found++;
        }
    }
    return -1;
}

int8_t MelodicSequencer::getNoteAt(uint8_t step, uint8_t index) const {
    int idx = findStoredNoteIndex(step, index);
    if (idx < 0) return -1;
    return notes[step][idx];
}

uint8_t MelodicSequencer::getVelocityAt(uint8_t step, uint8_t index) const {
    int idx = findStoredNoteIndex(step, index);
    if (idx < 0) return 0;
    return velocities[step][idx];
}

uint8_t MelodicSequencer::getCCNumberAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < ccCount[step]) return ccNumber[step][index];
    return 0;
}

uint8_t MelodicSequencer::getCCValueAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < ccCount[step]) return ccValue[step][index];
    return 0;
}

int8_t MelodicSequencer::getTranspose(uint8_t step) const {
    if (step < MAX_SEQ_STEPS) return transpose[step];
    return 0;
}

void MelodicSequencer::setTranspose(uint8_t step, int8_t value) {
    if (step < MAX_SEQ_STEPS) {
        transpose[step] = constrain(value, -24, 24);
        patternDirty = true;
        scheduleGlobalSave();
    }
}

void MelodicSequencer::adjustTranspose(uint8_t step, int8_t delta) {
    if (step < MAX_SEQ_STEPS) {
        transpose[step] = constrain(transpose[step] + delta, -24, 24);
        patternDirty = true;
        scheduleGlobalSave();
    }
}

// === Pattern file I/O ===

bool MelodicSequencer::saveToFile(uint8_t slot) {
    if (slot >= MAX_PATTERNS) return false;
    if (!LittleFS.begin(true)) return false;
    if (!LittleFS.exists(PATTERN_DIR)) LittleFS.mkdir(PATTERN_DIR);

    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", slot);

    File f = LittleFS.open(path, "w");
    if (!f) return false;

    const char magic[4] = {'M', 'S', 'E', 'Q'};
    uint8_t version = PATTERN_FILE_VERSION;
    f.write((uint8_t*)magic, 4);
    f.write(&version, 1);

    MelSeqParams saveParams = params;
    saveParams.bpm = 0;
    saveParams.channel = 0;
    f.write((uint8_t*)&saveParams, sizeof(MelSeqParams));
    f.write((uint8_t*)notes, sizeof(notes));
    f.write((uint8_t*)velocities, sizeof(velocities));
    f.write((uint8_t*)noteCount, sizeof(noteCount));
    f.write((uint8_t*)ccNumber, sizeof(ccNumber));
    f.write((uint8_t*)ccValue, sizeof(ccValue));
    f.write((uint8_t*)ccCount, sizeof(ccCount));
    f.write((uint8_t*)tie, sizeof(tie));
    f.write((uint8_t*)transpose, sizeof(transpose));
    f.write((uint8_t*)channels, sizeof(channels));
    f.write((uint8_t*)lengthTicks, sizeof(lengthTicks));

    f.close();
    currentPattern = slot;
    patternDirty = false;
    return true;
}

bool MelodicSequencer::loadFromFile(uint8_t slot) {
    if (slot >= MAX_PATTERNS) return false;
    if (!LittleFS.begin(true)) return false;

    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", slot);
    initArrays();

    if (!LittleFS.exists(path)) {
        currentPattern = slot;
        patternDirty = false;
        return true;
    }

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    char magic[4];
    uint8_t version = 0;
    f.read((uint8_t*)magic, 4);
    f.read(&version, 1);
    if (magic[0] != 'M' || magic[1] != 'S' || magic[2] != 'E' || magic[3] != 'Q') {
        f.close();
        return false;
    }

    MelSeqParams loadedParams;
    f.read((uint8_t*)&loadedParams, sizeof(MelSeqParams));

    uint8_t savedBpm = params.bpm;
    uint8_t savedChannel = params.channel;
    uint8_t savedMode = params.mode;
    uint8_t savedStrum = params.strum;
    uint8_t savedReRec = params.reRec;
    uint8_t savedGate = params.gate;
    uint8_t savedSwing = params.swing;
    uint8_t savedRandomness = params.randomness;
    uint8_t savedProbability = params.probability;
    params = loadedParams;
    params.bpm = savedBpm;
    params.channel = savedChannel;
    params.mode = savedMode;
    params.strum = savedStrum;
    params.reRec = savedReRec;
    params.gate = savedGate;
    params.swing = savedSwing;
    params.randomness = savedRandomness;
    params.probability = savedProbability;

    f.read((uint8_t*)notes, sizeof(notes));
    f.read((uint8_t*)velocities, sizeof(velocities));
    f.read((uint8_t*)noteCount, sizeof(noteCount));
    f.read((uint8_t*)ccNumber, sizeof(ccNumber));
    f.read((uint8_t*)ccValue, sizeof(ccValue));
    f.read((uint8_t*)ccCount, sizeof(ccCount));
    f.read((uint8_t*)tie, sizeof(tie));
    f.read((uint8_t*)transpose, sizeof(transpose));

    if (version >= 2) {
        f.read((uint8_t*)channels, sizeof(channels));
        f.read((uint8_t*)lengthTicks, sizeof(lengthTicks));
    } else {
        for (int s = 0; s < MAX_SEQ_STEPS; s++) {
            for (int n = 0; n < MAX_POLY; n++) {
                channels[s][n] = 1;
                lengthTicks[s][n] = 0;
            }
        }
    }

    f.close();

    uint8_t savedStep = currentStep;
    recording = 0;
    currentStep = savedStep;
    editStep = currentStep;
    direction = 1;
    ticksIntoStep = 0;
    noteHeld = false;
    stepEditActive = false;
    transposeEditActive = false;
    currentPattern = slot;
    patternDirty = false;
    return true;
}

bool MelodicSequencer::getStepData(uint8_t patternSlot, uint8_t step,
    int8_t* outNotes, uint8_t* outVelocities, uint8_t& outNoteCount,
    uint8_t* outCCNum, uint8_t* outCCVal, uint8_t& outCCCount,
    bool& outTie, int8_t& outTranspose, uint8_t& outLength) {

    if (patternSlot >= MAX_PATTERNS || step >= MAX_SEQ_STEPS) return false;
    if (!LittleFS.begin(true)) return false;

    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", patternSlot);
    if (!LittleFS.exists(path)) {
        outNoteCount = 0;
        outCCCount = 0;
        outTie = false;
        outTranspose = 0;
        outLength = 16;
        return true;
    }

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    f.seek(5);
    MelSeqParams fileParams;
    f.read((uint8_t*)&fileParams, sizeof(MelSeqParams));
    outLength = fileParams.length;

    if (step >= outLength) {
        outNoteCount = 0;
        outCCCount = 0;
        outTie = false;
        outTranspose = 0;
        f.close();
        return true;
    }

    uint32_t baseOffset = 5 + sizeof(MelSeqParams);
    uint32_t notesOffset = baseOffset;
    uint32_t velocitiesOffset = notesOffset + sizeof(notes);
    uint32_t noteCountOffset = velocitiesOffset + sizeof(velocities);
    uint32_t ccNumberOffset = noteCountOffset + sizeof(noteCount);
    uint32_t ccValueOffset = ccNumberOffset + sizeof(ccNumber);
    uint32_t ccCountOffset = ccValueOffset + sizeof(ccValue);
    uint32_t tieOffset = ccCountOffset + sizeof(ccCount);
    uint32_t transposeOffset = tieOffset + sizeof(tie);

    f.seek(noteCountOffset + step);
    f.read(&outNoteCount, 1);

    int8_t stepNotes[MAX_POLY];
    uint8_t stepVelocities[MAX_POLY];
    f.seek(notesOffset + step * MAX_POLY);
    f.read((uint8_t*)stepNotes, MAX_POLY);
    f.seek(velocitiesOffset + step * MAX_POLY);
    f.read(stepVelocities, MAX_POLY);
    for (int i = 0; i < MAX_POLY; i++) {
        outNotes[i] = stepNotes[i];
        outVelocities[i] = stepVelocities[i];
    }

    f.seek(ccCountOffset + step);
    f.read(&outCCCount, 1);

    uint8_t stepCCNum[MAX_CC_PER_STEP];
    uint8_t stepCCVal[MAX_CC_PER_STEP];
    f.seek(ccNumberOffset + step * MAX_CC_PER_STEP);
    f.read(stepCCNum, MAX_CC_PER_STEP);
    f.seek(ccValueOffset + step * MAX_CC_PER_STEP);
    f.read(stepCCVal, MAX_CC_PER_STEP);
    for (int i = 0; i < MAX_CC_PER_STEP; i++) {
        outCCNum[i] = stepCCNum[i];
        outCCVal[i] = stepCCVal[i];
    }

    f.seek(tieOffset + step);
    f.read((uint8_t*)&outTie, 1);
    f.seek(transposeOffset + step);
    f.read((uint8_t*)&outTranspose, 1);

    f.close();
    return true;
}
