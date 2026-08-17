#include "seq_mel.h"
#include <MIDI.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "hardware.h"
#include "midi_handler.h"
#include "clock_engine.h"


void MelodicSequencer::begin() {
    initArrays();
    
    // Инициализация LittleFS
    if (!LittleFS.begin(true)) {
        // Fallback: если не удалось — работаем без сохранения паттернов
    }
    
    editStep = 0;
    // Паттерн загружается в loadAllSettings() после установки глобальных параметров
}

void MelodicSequencer::initArrays() {
    currentPattern = 0;
    patternDirty = false;
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
        for (int j = 0; j < MAX_POLY; j++) {
            notes[i][j] = -1;
            velocities[i][j] = 100;
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
    // Шаг секвенсора = 1/16 ноты = 6 PPQN
    uint16_t base = clock_ticksPerDivision(4);
    if (params.swing == 0 || base < 2) return base;

    uint16_t swingAmt = (uint16_t)(((uint32_t)base * params.swing * 50) / 12700);
    if (swingAmt >= base) swingAmt = base - 1;

    if (currentStep % 2 == 1) return base + swingAmt;
    uint16_t t = base - swingAmt;
    return t < 1 ? 1 : t;
}

void MelodicSequencer::doStep() {
    bool doRandomize = false;
    if (params.probability > 0 && params.randomness > 0) {
        if (random(127) < params.probability) {
            doRandomize = true;
        }
    }
    
    if (recording && noteHeld && !stepEditActive) {
        tie[currentStep] = true;
    }
    
    if (!tie[currentStep]) {
        stopAllNotes();
        playStep(currentStep, doRandomize);
    } else {
        for (int i = 0; i < lastPlayedCount; i++) {
            bool found = false;
            for (int j = 0; j < noteCount[currentStep]; j++) {
                if (notes[currentStep][j] == lastPlayedNotes[i]) {
                    found = true;
                    break;
                }
            }
            if (!found && noteCount[currentStep] > 0) {
                MIDI.sendNoteOff(lastPlayedNotes[i], 0, params.channel);
            }
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

void MelodicSequencer::onClockTick() {
    if (!enabled) return;
    if (params.length == 0) return;

    ticksIntoStep++;
    if (ticksIntoStep >= stepTicks()) {
        ticksIntoStep = 0;
        doStep();
    }
}

void MelodicSequencer::playStep(uint8_t step, bool doRandomize) {
    int8_t rndAmt = doRandomize ? (params.randomness / 2) : 0;
    
    // CC всегда проигрываются, даже на пустом шаге с Tie
    for (int i = 0; i < ccCount[step]; i++) {
        uint8_t ccVal = ccValue[step][i];
        if (doRandomize) {
            int16_t delta = random(-rndAmt, rndAmt + 1);
            ccVal = constrain((int16_t)ccVal + delta, 0, 127);
        }
        MIDI.sendControlChange(ccNumber[step][i], ccVal, params.channel);
    }
    
    // Если шаг пустой и Tie ON — не играем ноты, оставляем предыдущие звучать
    if (noteCount[step] == 0 && tie[step]) {
        return;
    }
    
    // Проверка на mute шага — только при активной рандомизации
    if (doRandomize) {
        uint8_t muteChance = params.randomness / 4;  // до 31 при RND=127
        if (muteChance > 0 && random(127) < muteChance) {
            return; // Пропускаем шаг
        }
    }
    
    lastPlayedCount = 0;
    
    // Проверяем Tie с предыдущего шага
    uint8_t prevStep = (step == 0) ? params.length - 1 : step - 1;
    bool hasTieFromPrev = tie[prevStep] && noteCount[prevStep] > 0;
    
    for (int i = 0; i < noteCount[step]; i++) {
        if (notes[step][i] >= 0 && notes[step][i] < 128) {
            // Пропускаем NoteOn если Tie с предыдущего и нота уже звучит
            bool skipNoteOn = false;
            if (hasTieFromPrev) {
                for (int j = 0; j < lastPlayedCount; j++) {
                    if (lastPlayedNotes[j] == notes[step][i]) {
                        skipNoteOn = true;
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
                int16_t transposedNote = (int16_t)notes[step][i] + transpose[step];
                transposedNote = constrain(transposedNote, 0, 127);
                MIDI.sendNoteOn((uint8_t)transposedNote, vel, params.channel);
            }
            // Обновляем lastPlayedNotes
            if (lastPlayedCount < MAX_POLY) {
                lastPlayedNotes[lastPlayedCount++] = notes[step][i];
            }
        }
    }
}

void MelodicSequencer::stopAllNotes() {
    for (int i = 0; i < lastPlayedCount; i++) {
        MIDI.sendNoteOff(lastPlayedNotes[i], 0, params.channel);
    }
    lastPlayedCount = 0;
}

bool MelodicSequencer::handleNoteOn(uint8_t note, uint8_t velocity) {
    if (params.strum == 1) {
        MIDI.sendNoteOn(note, velocity, params.channel);
    }
    
    if (!recording) return false;
    
    uint8_t targetStep = stepEditActive ? editStep : currentStep;
    if (targetStep >= MAX_SEQ_STEPS) return true;
    
    unsigned long now = millis();
    bool newChord = (now - lastChordTime > 80);
    
    if (params.reRec == 1 && newChord && !noteHeld) {
        for (int j = 0; j < MAX_POLY; j++) {
            notes[targetStep][j] = -1;
            velocities[targetStep][j] = 100;
        }
        noteCount[targetStep] = 0;
        tie[targetStep] = false;
    }
    
    lastChordTime = now;
    
    // Не добавляем ноту если она уже есть в этом шаге (Tie продолжение)
    for (int j = 0; j < noteCount[targetStep]; j++) {
        if (notes[targetStep][j] == note) {
            velocities[targetStep][j] = velocity;
            noteHeld = true;
            scheduleGlobalSave();
            return (params.strum == 1);
        }
    }
    
    if (noteCount[targetStep] >= MAX_POLY) {
        for (int j = 0; j < MAX_POLY - 1; j++) {
            notes[targetStep][j] = notes[targetStep][j + 1];
            velocities[targetStep][j] = velocities[targetStep][j + 1];
        }
        noteCount[targetStep] = MAX_POLY - 1;
    }
    
    notes[targetStep][noteCount[targetStep]] = note;
    velocities[targetStep][noteCount[targetStep]] = velocity;
    noteCount[targetStep]++;
    noteHeld = true;
    patternDirty = true;
    scheduleGlobalSave();
    
    return (params.strum == 1);
}

bool MelodicSequencer::handleNoteOff(uint8_t note) {
    if (params.strum == 1) {
        MIDI.sendNoteOff(note, 0, params.channel);
    }
    if (recording) {
        noteHeld = false;
        return true;
    }
    return false;
}

bool MelodicSequencer::handleCC(uint8_t number, uint8_t value) {
    if (params.strum == 1) {
        MIDI.sendControlChange(number, value, params.channel);
    }
    
    if (!recording) return false;
    
    uint8_t targetStep = stepEditActive ? editStep : currentStep;
    if (targetStep >= MAX_SEQ_STEPS) return true;
    
    // Поиск существующего CC с таким же номером в текущем шаге
    for (int i = 0; i < ccCount[targetStep]; i++) {
        if (ccNumber[targetStep][i] == number) {
            // Обновляем значение, не меняя порядок и количество
            ccValue[targetStep][i] = value;
            patternDirty = true;
            scheduleGlobalSave();
            return (params.strum == 1);
        }
    }
    
    // CC с таким номером не найден — добавляем новый слот
    if (ccCount[targetStep] >= MAX_CC_PER_STEP) {
        // Вытесняем первый слот, сдвигаем остальные влево
        for (int j = 0; j < MAX_CC_PER_STEP - 1; j++) {
            ccNumber[targetStep][j] = ccNumber[targetStep][j + 1];
            ccValue[targetStep][j] = ccValue[targetStep][j + 1];
        }
        ccCount[targetStep] = MAX_CC_PER_STEP - 1;
    }
    
    // Пишем новый CC в последний слот
    ccNumber[targetStep][ccCount[targetStep]] = number;
    ccValue[targetStep][ccCount[targetStep]] = value;
    ccCount[targetStep]++;
    patternDirty = true;
    scheduleGlobalSave();
    
    return (params.strum == 1);
}

void MelodicSequencer::play() {
    if (enabled) return;
    enabled = true;
    resetClockPhase();
    direction = 1;
    if (params.mode == 1) currentStep = params.length - 1;
    else currentStep = 0;
    editStep = currentStep;
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void MelodicSequencer::stop() {
    if (!enabled) return;
    enabled = false;
    stopAllNotes();
    MIDI.sendControlChange(123, 0, params.channel);
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
    initArrays();
    stopAllNotes();
    patternDirty = true;
    scheduleGlobalSave();
}

void MelodicSequencer::clearStep(uint8_t step) {
    if (step < MAX_SEQ_STEPS) {
        for (int j = 0; j < MAX_POLY; j++) {
            notes[step][j] = -1;
            velocities[step][j] = 100;
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

int8_t MelodicSequencer::getNoteAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < noteCount[step]) {
        return notes[step][index];
    }
    return -1;
}

uint8_t MelodicSequencer::getVelocityAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < noteCount[step]) {
        return velocities[step][index];
    }
    return 0;
}

uint8_t MelodicSequencer::getCCNumberAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < ccCount[step]) {
        return ccNumber[step][index];
    }
    return 0;
}

uint8_t MelodicSequencer::getCCValueAt(uint8_t step, uint8_t index) const {
    if (step < MAX_SEQ_STEPS && index < ccCount[step]) {
        return ccValue[step][index];
    }
    return 0;
}

int8_t MelodicSequencer::getTranspose(uint8_t step) const {
    if (step < MAX_SEQ_STEPS) {
        return transpose[step];
    }
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

bool MelodicSequencer::getStepData(uint8_t patternSlot, uint8_t step,
    int8_t* outNotes, uint8_t* outVelocities, uint8_t& outNoteCount,
    uint8_t* outCCNum, uint8_t* outCCVal, uint8_t& outCCCount,
    bool& outTie, int8_t& outTranspose, uint8_t& outLength) {
    
    if (patternSlot >= MAX_PATTERNS || step >= MAX_SEQ_STEPS) return false;
    
    if (!LittleFS.begin(true)) return false;
    
    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", patternSlot);
    
    if (!LittleFS.exists(path)) {
        // Файл не существует — возвращаем пустой шаг
        outNoteCount = 0;
        outCCCount = 0;
        outTie = false;
        outTranspose = 0;
        outLength = 16;
        return true;
    }
    
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    // Пропускаем заголовок (4 байта магия + 1 байт версия)
    f.seek(5);
    
    // Читаем MelSeqParams чтобы получить length паттерна
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
    
    // Вычисляем позицию данных шага
    // После params идут: notes, velocities, noteCount, ccNumber, ccValue, ccCount, tie, transpose
    uint32_t baseOffset = 5 + sizeof(MelSeqParams);
    uint32_t notesOffset = baseOffset;
    uint32_t velocitiesOffset = notesOffset + sizeof(notes);
    uint32_t noteCountOffset = velocitiesOffset + sizeof(velocities);
    uint32_t ccNumberOffset = noteCountOffset + sizeof(noteCount);
    uint32_t ccValueOffset = ccNumberOffset + sizeof(ccNumber);
    uint32_t ccCountOffset = ccValueOffset + sizeof(ccValue);
    uint32_t tieOffset = ccCountOffset + sizeof(ccCount);
    uint32_t transposeOffset = tieOffset + sizeof(tie);
    
    // Читаем noteCount для шага
    f.seek(noteCountOffset + step);
    f.read(&outNoteCount, 1);
    
    // Читаем ноты и velocities для шага
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
    
    // Читаем ccCount для шага
    f.seek(ccCountOffset + step);
    f.read(&outCCCount, 1);
    
    // Читаем CC для шага
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
    
    // Читаем tie
    f.seek(tieOffset + step);
    f.read((uint8_t*)&outTie, 1);
    
    // Читаем transpose
    f.seek(transposeOffset + step);
    f.read((uint8_t*)&outTranspose, 1);
    
    f.close();
    return true;
}

bool MelodicSequencer::saveToFile(uint8_t slot) {
    if (slot >= MAX_PATTERNS) return false;
    
    if (!LittleFS.begin(true)) return false;
    
    // Создаём директорию если нет
    if (!LittleFS.exists(PATTERN_DIR)) {
        LittleFS.mkdir(PATTERN_DIR);
    }
    
    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", slot);
    
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    
    // Заголовок
    const char magic[4] = {'M', 'S', 'E', 'Q'};
    uint8_t version = 1;
    f.write((uint8_t*)magic, 4);
    f.write(&version, 1);
    
    // Данные (сохраняем params, но bpm и channel сохраняем как 0 — они глобальные)
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
    
    // Всегда начинаем с чистого состояния перед загрузкой
    initArrays();
    
    if (!LittleFS.exists(path)) {
        // Файл не существует — оставляем пустой паттерн
        currentPattern = slot;
        patternDirty = false;
        return true;
    }
    
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    // Заголовок
    char magic[4];
    uint8_t version;
    f.read((uint8_t*)magic, 4);
    f.read(&version, 1);
    
    if (magic[0] != 'M' || magic[1] != 'S' || magic[2] != 'E' || magic[3] != 'Q') {
        f.close();
        return false;
    }
    
    // Данные
    MelSeqParams loadedParams;
    f.read((uint8_t*)&loadedParams, sizeof(MelSeqParams));
    
    // Глобальные параметры — не перезаписываем из файла паттерна
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
    
    f.close();
    
    // Сбрасываем runtime-состояние, но сохраняем позицию
    uint8_t savedStep = currentStep;
    
    recording = 0;
    currentStep = savedStep;
    editStep = currentStep;
    direction = 1;
    ticksIntoStep = 0;
    lastPlayedCount = 0;
    noteHeld = false;
    stepEditActive = false;
    transposeEditActive = false;
    
    currentPattern = slot;
    patternDirty = false;
    return true;
}