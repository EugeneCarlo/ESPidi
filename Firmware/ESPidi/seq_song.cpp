#include "seq_song.h"
#include "seq_mel.h"
#include <MIDI.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "hardware.h"
#include "midi_handler.h"
#include "clock_engine.h"

extern MelodicSequencer melSeq;

void SongSequencer::begin() {
    initArrays();
    
    if (!LittleFS.begin(true)) {
        // Fallback
    }
    
    editStep = 0;
    // Песня загружается в loadAllSettings()
}

void SongSequencer::initArrays() {
    currentSong = 0;
    songDirty = false;
    for (int i = 0; i < MAX_SONG_STEPS; i++) {
        steps[i].patternSlot = 0;
        steps[i].transpose = 0;
        steps[i].divider = 2;       // 1/4 по умолчанию
        steps[i].pauseLength = 16;  // 16 шагов паузы по умолчанию
        steps[i].mute = false;
    }
    currentStep = 0;
    patternPlayStep = 0;
    patternPlayLength = 0;
    currentPatternSlot = 0;
    direction = 1;
    
    // Сбрасываем Tie-состояние
    lastPatternStep = 255;
    lastPatternSlotForTie = 255;
    lastPlayedCountForTie = 0;
}

bool SongSequencer::loadPatternToBuffer(uint8_t slot) {
    if (slot == patternBufferSlot) return true;  // уже загружен
    if (slot == 0 || slot > 64) {
        patternBufferSlot = 255;
        patternBufferLength = 0;
        return false;
    }
    
    // Загружаем паттерн через getStepData — но нам нужен весь паттерн
    // Временное решение: читаем файл напрямую
    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", slot - 1);
    
    if (!LittleFS.exists(path)) {
        patternBufferSlot = 255;
        patternBufferLength = 0;
        return false;
    }
    
    // Очищаем буферы перед загрузкой
    memset(patternBuffer_notes, 0, sizeof(patternBuffer_notes));
    memset(patternBuffer_velocities, 0, sizeof(patternBuffer_velocities));
    memset(patternBuffer_noteCount, 0, sizeof(patternBuffer_noteCount));
    memset(patternBuffer_ccNumber, 0, sizeof(patternBuffer_ccNumber));
    memset(patternBuffer_ccValue, 0, sizeof(patternBuffer_ccValue));
    memset(patternBuffer_ccCount, 0, sizeof(patternBuffer_ccCount));
    memset(patternBuffer_tie, 0, sizeof(patternBuffer_tie));
    memset(patternBuffer_transpose, 0, sizeof(patternBuffer_transpose));
    
    File f = LittleFS.open(path, "r");
    if (!f) {
        patternBufferSlot = 255;
        patternBufferLength = 0;
        return false;
    }
    
    // Пропускаем заголовок (5 байт)
    f.seek(5);
    
    // Читаем params чтобы получить length
    MelSeqParams fileParams;
    f.read((uint8_t*)&fileParams, sizeof(MelSeqParams));
    patternBufferLength = fileParams.length;
    
    // Читаем все данные
    f.read((uint8_t*)patternBuffer_notes, sizeof(patternBuffer_notes));
    f.read((uint8_t*)patternBuffer_velocities, sizeof(patternBuffer_velocities));
    f.read((uint8_t*)patternBuffer_noteCount, sizeof(patternBuffer_noteCount));
    f.read((uint8_t*)patternBuffer_ccNumber, sizeof(patternBuffer_ccNumber));
    f.read((uint8_t*)patternBuffer_ccValue, sizeof(patternBuffer_ccValue));
    f.read((uint8_t*)patternBuffer_ccCount, sizeof(patternBuffer_ccCount));
    f.read((uint8_t*)patternBuffer_tie, sizeof(patternBuffer_tie));
    f.read((uint8_t*)patternBuffer_transpose, sizeof(patternBuffer_transpose));
    
    f.close();
    patternBufferSlot = slot;
    return true;
}


void SongSequencer::update() {
    // Timing — onClockTick
}

void SongSequencer::resetClockPhase() {
    ticksIntoStep = 0;
}

uint16_t SongSequencer::patternStepTicks(uint8_t divider) const {
    return clock_ticksPerDivision(divider);
}

void SongSequencer::advanceSongStep() {
    switch (params.mode) {
        case 0: currentStep = (currentStep + 1) % params.length; break;
        case 1: currentStep = (currentStep - 1 + params.length) % params.length; break;
        case 2:
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

    if (currentStep == 0 && params.cycle == 0) {
        enabled = false;
        stopAllNotes();
        resetClockPhase();
        extern void ui_markDirty(uint8_t flags);
        ui_markDirty(1);
        return;
    }

    patternPlayStep = 0;
    patternPlayLength = 0;
    currentPatternSlot = 0;
    lastPatternStep = 255;
    lastPlayedCountForTie = 0;
    ticksIntoStep = 0;
}

void SongSequencer::doPatternPulse() {
    SongStepParams& currentSongStep = steps[currentStep];
    playPatternStep(patternPlayStep, currentSongStep.transpose);
    patternPlayStep++;

    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(2);

    if (patternPlayStep >= patternPlayLength) {
        stopAllNotes();
        advanceSongStep();
        lastPatternStep = 255;
        lastPlayedCountForTie = 0;
    }
}

void SongSequencer::onClockTick() {
    if (!enabled) return;
    if (params.length == 0) return;

    SongStepParams& currentSongStep = steps[currentStep];

    // MUTE / пауза: ждём pauseLength * stepTicks
    if (currentSongStep.mute || currentSongStep.patternSlot == 0) {
        uint16_t need = (uint16_t)currentSongStep.pauseLength *
                        patternStepTicks(currentSongStep.divider);
        if (need < 1) need = 1;

        ticksIntoStep++;
        if (ticksIntoStep >= need) {
            if (currentSongStep.mute) stopAllNotes();
            advanceSongStep();
        }
        return;
    }

    // Паттерн
    uint8_t slot = currentSongStep.patternSlot;
    if (currentPatternSlot != slot) {
        currentPatternSlot = slot;
        patternPlayStep = 0;
        ticksIntoStep = 0;
        if (loadPatternToBuffer(slot)) {
            patternPlayLength = patternBufferLength;
        } else {
            patternPlayLength = 0;
        }
    }

    if (patternPlayLength == 0) {
        ticksIntoStep++;
        if (ticksIntoStep >= patternStepTicks(currentSongStep.divider)) {
            advanceSongStep();
        }
        return;
    }

    ticksIntoStep++;
    uint16_t need = patternStepTicks(currentSongStep.divider);
    if (ticksIntoStep >= need) {
        ticksIntoStep = 0;
        doPatternPulse();
    }
}

void SongSequencer::playPatternStep(uint8_t step, int8_t transpose) {
    if (patternBufferSlot == 255 || step >= patternBufferLength) return;
    
    // Если сменился паттерн — сбрасываем Tie
    if (patternBufferSlot != lastPatternSlotForTie) {
        lastPatternStep = 255;
        lastPlayedCountForTie = 0;
        lastPatternSlotForTie = patternBufferSlot;
    }
    
    // Проверяем Tie с предыдущего шага
    bool hasTieFromPrev = false;
    if (lastPatternStep != 255 && lastPatternStep < patternBufferLength) {
        if (patternBuffer_tie[lastPatternStep]) {
            uint8_t expectedPrevStep = (step == 0) ? patternBufferLength - 1 : step - 1;
            if (lastPatternStep == expectedPrevStep) {
                hasTieFromPrev = true;
            }
        }
    }
    
    // CC всегда проигрываются
    for (int i = 0; i < patternBuffer_ccCount[step]; i++) {
        MIDI.sendControlChange(patternBuffer_ccNumber[step][i], patternBuffer_ccValue[step][i], params.channel);
    }
    
    // Если шаг пустой и Tie ON — оставляем предыдущие ноты звучать
    if (patternBuffer_noteCount[step] == 0 && patternBuffer_tie[step]) {
        lastPatternStep = step;
        return;
    }
    
    if (!hasTieFromPrev) {
        // Нет Tie с предыдущего — останавливаем все ноты
        for (int i = 0; i < lastPlayedCountForTie; i++) {
            MIDI.sendNoteOff(lastPlayedNotesForTie[i], 0, params.channel);
        }
        lastPlayedCountForTie = 0;
    } else {
        // Tie: останавливаем только ноты, которых нет в текущем шаге
        uint8_t newLastPlayedCount = 0;
        int8_t newLastPlayedNotes[MAX_POLY];
        
        for (int i = 0; i < lastPlayedCountForTie; i++) {
            bool found = false;
            for (int j = 0; j < patternBuffer_noteCount[step]; j++) {
                int16_t transposedNote = (int16_t)patternBuffer_notes[step][j] + transpose + patternBuffer_transpose[step];
                transposedNote = constrain(transposedNote, 0, 127);
                if ((uint8_t)transposedNote == lastPlayedNotesForTie[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                MIDI.sendNoteOff(lastPlayedNotesForTie[i], 0, params.channel);
            } else {
                if (newLastPlayedCount < MAX_POLY) {
                    newLastPlayedNotes[newLastPlayedCount++] = lastPlayedNotesForTie[i];
                }
            }
        }
        
        lastPlayedCountForTie = newLastPlayedCount;
        for (int i = 0; i < newLastPlayedCount; i++) {
            lastPlayedNotesForTie[i] = newLastPlayedNotes[i];
        }
    }
    
    // Играем ноты текущего шага
    for (int i = 0; i < patternBuffer_noteCount[step]; i++) {
        int8_t note = patternBuffer_notes[step][i];
        if (note >= 0 && note < 128) {
            int16_t transposedNote = (int16_t)note + transpose + patternBuffer_transpose[step];
            transposedNote = constrain(transposedNote, 0, 127);
            
            // Проверяем, не звучит ли уже эта нота из-за Tie
            bool alreadyPlaying = false;
            if (hasTieFromPrev) {
                for (int j = 0; j < lastPlayedCountForTie; j++) {
                    if (lastPlayedNotesForTie[j] == (uint8_t)transposedNote) {
                        alreadyPlaying = true;
                        break;
                    }
                }
            }
            
            if (!alreadyPlaying) {
                MIDI.sendNoteOn((uint8_t)transposedNote, patternBuffer_velocities[step][i], params.channel);
            }
            
            // Добавляем в список играющих нот (избегаем дубликатов)
            bool duplicate = false;
            for (int j = 0; j < lastPlayedCountForTie; j++) {
                if (lastPlayedNotesForTie[j] == (uint8_t)transposedNote) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate && lastPlayedCountForTie < MAX_POLY) {
                lastPlayedNotesForTie[lastPlayedCountForTie++] = (uint8_t)transposedNote;
            }
        }
    }
    
    lastPatternStep = step;
}


void SongSequencer::stopAllNotes() {
    for (int i = 0; i < 128; i++) {
        MIDI.sendNoteOff(i, 0, params.channel);
    }
    MIDI.sendControlChange(123, 0, params.channel);
    patternBufferSlot = 255;
}

void SongSequencer::play() {
    if (enabled) return;
    enabled = true;
    resetClockPhase();
    direction = 1;
    currentStep = 0;
    patternPlayStep = 0;
    patternPlayLength = 0;
    currentPatternSlot = 0;
    patternBufferSlot = 255;
    lastPatternStep = 255;
    lastPatternSlotForTie = 255;
    lastPlayedCountForTie = 0;
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void SongSequencer::stop() {
    if (!enabled) return;
    enabled = false;
    stopAllNotes();
    MIDI.sendControlChange(123, 0, params.channel);
    resetClockPhase();
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void SongSequencer::toggle() {
    if (enabled) stop();
    else play();
}

void SongSequencer::tap() {
    if (clock_isSourceExternal()) return;
    unsigned long now = millis();
    if (lastTapTime > 0 && (now - lastTapTime) < TAP_TIMEOUT_MS) {
        uint16_t newBpm = 60000 / (now - lastTapTime);
        clock_setBpm(constrain(newBpm, 40, 250));
    }
    lastTapTime = now;
}

void SongSequencer::clear() {
    initArrays();
    stopAllNotes();
    songDirty = true;
    scheduleGlobalSave();
}

void SongSequencer::clearStep(uint8_t step) {
    if (step < MAX_SONG_STEPS) {
        steps[step].patternSlot = 0;
        steps[step].transpose = 0;
        steps[step].divider = 2;       // 1/4
        steps[step].pauseLength = 16;  // 16 шагов
        steps[step].mute = false;
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::setPatternSlot(uint8_t step, uint8_t slot) {
    if (step < MAX_SONG_STEPS && slot <= 64) {
        steps[step].patternSlot = slot;
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::setTranspose(uint8_t step, int8_t value) {
    if (step < MAX_SONG_STEPS) {
        steps[step].transpose = constrain(value, -24, 24);
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::adjustTranspose(uint8_t step, int8_t delta) {
    if (step < MAX_SONG_STEPS) {
        steps[step].transpose = constrain(steps[step].transpose + delta, -24, 24);
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::setDivider(uint8_t step, uint8_t value) {
    if (step < MAX_SONG_STEPS && value <= 5) {
        steps[step].divider = value;
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::setPauseLength(uint8_t step, uint8_t value) {
    if (step < MAX_SONG_STEPS && value >= 1 && value <= 64) {
        steps[step].pauseLength = value;
        songDirty = true;
        scheduleGlobalSave();
    }
}

void SongSequencer::toggleMute(uint8_t step) {
    if (step < MAX_SONG_STEPS) {
        steps[step].mute = !steps[step].mute;
        songDirty = true;
        scheduleGlobalSave();
    }
}

bool SongSequencer::saveToFile(uint8_t slot) {
    if (slot >= MAX_SONGS) return false;
    
    if (!LittleFS.begin(true)) return false;
    
    if (!LittleFS.exists(SONG_DIR)) {
        LittleFS.mkdir(SONG_DIR);
    }
    
    char path[32];
    snprintf(path, sizeof(path), SONG_DIR "/song_%02d.bin", slot);
    
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    
    const char magic[4] = {'S', 'O', 'N', 'G'};
    uint8_t version = 1;
    f.write((uint8_t*)magic, 4);
    f.write(&version, 1);
    f.write(&params.length, 1);
    f.write((uint8_t*)steps, sizeof(steps));
    
    f.close();
    currentSong = slot;
    songDirty = false;
    return true;
}

bool SongSequencer::loadFromFile(uint8_t slot) {
    if (slot >= MAX_SONGS) return false;
    
    if (!LittleFS.begin(true)) return false;
    
    char path[32];
    snprintf(path, sizeof(path), SONG_DIR "/song_%02d.bin", slot);
    
    initArrays();
    
    if (!LittleFS.exists(path)) {
        currentSong = slot;
        songDirty = false;
        return true;
    }
    
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    char magic[4];
    uint8_t version;
    f.read((uint8_t*)magic, 4);
    f.read(&version, 1);
    
    if (magic[0] != 'S' || magic[1] != 'O' || magic[2] != 'N' || magic[3] != 'G') {
        f.close();
        return false;
    }
    
    f.read(&params.length, 1);
    f.read((uint8_t*)steps, sizeof(steps));
    
    f.close();
    
    currentStep = 0;
    editStep = 0;
    direction = 1;
    ticksIntoStep = 0;
    patternPlayStep = 0;
    patternPlayLength = 0;
    currentPatternSlot = 0;
    
    currentSong = slot;
    songDirty = false;
    return true;
}