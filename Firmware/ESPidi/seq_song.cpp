#include "seq_song.h"
#include "seq_mel.h"
#include <MIDI.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include "hardware.h"
#include "midi_handler.h"
#include "clock_engine.h"
#include "engine.h"

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
    direction = 1;
    
    // Сбрасываем Tie-состояние
    lastPatternStep = 255;
    lastPatternSlotForTie = 255;
    lastPlayedCountForTie = 0;
}

bool SongSequencer::loadPattern(SongPatternBuf& buf, uint8_t slot) {
    if (slot == buf.slot) return true;  // уже загружен
    buf.slot = 255;
    buf.length = 0;
    if (slot == 0 || slot > 64) return false;

    char path[32];
    snprintf(path, sizeof(path), PATTERN_DIR "/pat_%02d.bin", slot - 1);
    if (!LittleFS.exists(path)) return false;

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    memset(&buf, 0, sizeof(buf));
    f.seek(4);  // заголовок: MSEQ + версия
    uint8_t version = 0;
    f.read(&version, 1);
    MelSeqParams fileParams;
    f.read((uint8_t*)&fileParams, sizeof(MelSeqParams));
    f.read((uint8_t*)buf.notes, sizeof(buf.notes));
    f.read((uint8_t*)buf.velocities, sizeof(buf.velocities));
    f.read((uint8_t*)buf.noteCount, sizeof(buf.noteCount));
    f.read((uint8_t*)buf.ccNumber, sizeof(buf.ccNumber));
    f.read((uint8_t*)buf.ccValue, sizeof(buf.ccValue));
    f.read((uint8_t*)buf.ccCount, sizeof(buf.ccCount));
    f.read((uint8_t*)buf.tie, sizeof(buf.tie));
    f.read((uint8_t*)buf.transpose, sizeof(buf.transpose));
    if (version >= 2) {
        f.seek(f.position() + sizeof(uint8_t) * MAX_SEQ_STEPS * MAX_POLY);  // каналы нот (песня играет на своём CH)
        f.read((uint8_t*)buf.lengthTicks, sizeof(buf.lengthTicks));
    }
    f.close();
    buf.gate = fileParams.gate;
    buf.length = fileParams.length;
    buf.slot = slot;
    return true;
}

// Паттерн для текущего шага: уже играет, заранее загружен (меняем буферы местами)
// или, если не успели, читаем сейчас.
bool SongSequencer::ensureCurrentPattern(uint8_t slot) {
    if (cur->slot == slot) return true;
    if (nxt->slot == slot) {
        SongPatternBuf* t = cur;
        cur = nxt;
        nxt = t;
        return true;
    }
    return loadPattern(*cur, slot);
}

// Шаг, с которого песня начинается по PLAY: в REV — последний (решение автора), иначе первый.
uint8_t SongSequencer::startStep() const {
    return (params.mode == 1 && params.length > 0) ? params.length - 1 : 0;
}

// Паттерн, который понадобится следующим и ещё не загружен (0 — ничего не нужно):
// во время игры — для следующего шага песни, в остановке — для стартового, чтобы PLAY
// не читал файл прямо в такте.
uint8_t SongSequencer::slotToPreload() const {
    uint8_t next = enabled ? plannedNext : startStep();
    if (next >= MAX_SONG_STEPS) return 0;
    uint8_t slot = steps[next].patternSlot;
    if (slot == 0 || cur->slot == slot || nxt->slot == slot) return 0;
    return slot;
}

void SongSequencer::update() {
    // Заранее загружаем паттерн следующего шага песни — здесь, вне обработки тактов.
    // Вызывается без блокировки движка: файл читается в отдельный буфер (~12 мс), пока такты
    // идут, а в буфер следующего паттерна копируется уже под блокировкой.
    static SongPatternBuf loaded;
    uint8_t slot;
    {
        EngineLock lock;
        slot = slotToPreload();
    }
    if (slot == 0) return;
    loaded.slot = 255;  // читать заново: файл могли пересохранить
    if (!loadPattern(loaded, slot)) return;
    EngineLock lock;
    if (slotToPreload() == slot) {
        *nxt = loaded;
    }
}

// Файл паттерна перезаписан (slot как в шагах песни: 1..64; 0 — все): загруженную копию
// больше не используем, update() перечитает.
void SongSequencer::patternChanged(uint8_t slot) {
    if (slot == 0 || bufA.slot == slot) bufA.slot = 255;
    if (slot == 0 || bufB.slot == slot) bufB.slot = 255;
}

void SongSequencer::resetClockPhase() {
    ticksIntoStep = 0;
}

uint16_t SongSequencer::patternStepTicks(uint8_t divider) const {
    return clock_ticksPerDivision(divider);
}

// Выбрать следующий шаг песни заранее (при входе в шаг), чтобы успеть загрузить его паттерн.
void SongSequencer::planNext() {
    int8_t dir = direction;
    int next = currentStep;
    switch (params.mode) {
        case 0: next = (currentStep + 1) % params.length; break;
        case 1: next = (currentStep - 1 + params.length) % params.length; break;
        case 2:
            next = currentStep + dir;
            if (next >= params.length || next < 0) {
                dir = -dir;
                next = currentStep + dir;
            }
            if (next < 0 || next >= params.length) next = 0;
            break;
        case 3: next = random(params.length); break;
    }
    plannedNext = (uint8_t)next;
    plannedDir = dir;
}

void SongSequencer::advanceSongStep() {
    stepsPlayed++;
    currentStep = plannedNext;
    direction = plannedDir;

    // CYCLE = OFF: песня заканчивается, когда сыграна целиком. В PEND — по возвращении на первый
    // шаг (проход туда и обратно), в остальных режимах — после LENGTH шагов: в RND шаг 0
    // выпадает случайно и не может быть признаком конца.
    bool finished = (params.mode == 2) ? (currentStep == 0) : (stepsPlayed >= params.length);
    if (finished && params.cycle == 0) {
        enabled = false;
        stopAllNotes();
        resetClockPhase();
        extern void ui_markDirty(uint8_t flags);
        ui_markDirty(1);
    }
}

// Начало шага песни: первый импульс паттерна звучит сразу, на этом же такте.
void SongSequencer::enterStep() {
    if (currentStep >= params.length) currentStep %= params.length;
    SongStepParams& s = steps[currentStep];
    ticksIntoStep = 0;
    patternPlayStep = 0;
    lastPatternStep = 255;
    planNext();
    if (s.patternSlot == 0) {
        patternPlayLength = 0;  // пауза
    } else {
        patternPlayLength = ensureCurrentPattern(s.patternSlot) ? cur->length : 0;
        if (!s.mute && patternPlayLength > 0) {
            playPatternStep(0, s.transpose);
        }
    }
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(2);
}

// Шаг песни отыграл полностью (включая последний шаг паттерна) — переходим к следующему.
void SongSequencer::nextSongStep() {
    stopAllNotes();
    advanceSongStep();
    if (enabled) enterStep();
}

void SongSequencer::onClockTick() {
    if (!enabled) return;
    if (params.length == 0) return;

    if (currentStep >= params.length) {
        currentStep %= params.length;  // LENGTH уменьшили на ходу — сразу возвращаемся в границы
    }

    tickNoteLengths();

    if (startPending) {  // первый такт после PLAY/Start — шаг песни начинается сразу
        startPending = false;
        enterStep();
        return;
    }

    SongStepParams& s = steps[currentStep];
    uint16_t div = patternStepTicks(s.divider);
    ticksIntoStep++;

    // Пауза, заглушённый (MUTE) или отсутствующий паттерн: просто ждём длительность шага.
    // Пауза — пустой паттерн из PAUSE шагов; MUTE — выключенный паттерн, длится как паттерн.
    if (s.patternSlot == 0 || s.mute || patternPlayLength == 0) {
        uint16_t n = (s.patternSlot == 0) ? s.pauseLength : (patternPlayLength ? patternPlayLength : 1);
        uint16_t need = n * div;
        if (need < 1) need = 1;
        if (ticksIntoStep >= need) nextSongStep();
        return;
    }

    if (ticksIntoStep >= div) {
        ticksIntoStep = 0;
        patternPlayStep++;
        if (patternPlayStep >= patternPlayLength) {
            nextSongStep();
        } else {
            playPatternStep(patternPlayStep, s.transpose);
            extern void ui_markDirty(uint8_t flags);
            ui_markDirty(2);
        }
    }
}

// Длина нот в песне — как в секвенсоре: GATE паттерна, записанная длина ноты, цепочка Tie.
void SongSequencer::tickNoteLengths() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < lastPlayedCountForTie; i++) {
        if (lastPlayedTicksLeft[i] > 0) lastPlayedTicksLeft[i]--;
        if (lastPlayedTicksLeft[i] == 0) {
            MIDI.sendNoteOff(lastPlayedNotesForTie[i], 0, params.channel);
            continue;
        }
        lastPlayedNotesForTie[n] = lastPlayedNotesForTie[i];
        lastPlayedTicksLeft[n] = lastPlayedTicksLeft[i];
        n++;
    }
    lastPlayedCountForTie = n;
}

void SongSequencer::playPatternStep(uint8_t step, int8_t transpose) {
    if (cur->slot == 255 || step >= cur->length) return;
    
    // Если сменился паттерн — сбрасываем Tie
    if (cur->slot != lastPatternSlotForTie) {
        lastPatternStep = 255;
        lastPlayedCountForTie = 0;
        lastPatternSlotForTie = cur->slot;
    }
    
    // Проверяем Tie с предыдущего шага
    bool hasTieFromPrev = false;
    if (lastPatternStep != 255 && lastPatternStep < cur->length) {
        if (cur->tie[lastPatternStep]) {
            uint8_t expectedPrevStep = (step == 0) ? cur->length - 1 : step - 1;
            if (lastPatternStep == expectedPrevStep) {
                hasTieFromPrev = true;
            }
        }
    }
    
    // CC всегда проигрываются
    for (int i = 0; i < cur->ccCount[step]; i++) {
        MIDI.sendControlChange(cur->ccNumber[step][i], cur->ccValue[step][i], params.channel);
    }
    
    // Если шаг пустой и Tie ON — оставляем предыдущие ноты звучать
    if (cur->noteCount[step] == 0 && cur->tie[step]) {
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
        int8_t newLastPlayedNotes[SONG_MAX_SOUNDING];
        uint16_t newTicksLeft[SONG_MAX_SOUNDING];
        
        for (int i = 0; i < lastPlayedCountForTie; i++) {
            bool found = false;
            for (int j = 0; j < cur->noteCount[step]; j++) {
                int16_t transposedNote = (int16_t)cur->notes[step][j] + transpose + cur->transpose[step];
                transposedNote = constrain(transposedNote, 0, 127);
                if ((uint8_t)transposedNote == lastPlayedNotesForTie[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                MIDI.sendNoteOff(lastPlayedNotesForTie[i], 0, params.channel);
            } else {
                if (newLastPlayedCount < SONG_MAX_SOUNDING) {
                    newTicksLeft[newLastPlayedCount] = lastPlayedTicksLeft[i];
                    newLastPlayedNotes[newLastPlayedCount++] = lastPlayedNotesForTie[i];
                }
            }
        }
        
        lastPlayedCountForTie = newLastPlayedCount;
        for (int i = 0; i < newLastPlayedCount; i++) {
            lastPlayedNotesForTie[i] = newLastPlayedNotes[i];
            lastPlayedTicksLeft[i] = newTicksLeft[i];
        }
    }
    
    // Играем ноты текущего шага
    for (int i = 0; i < cur->noteCount[step]; i++) {
        int8_t note = cur->notes[step][i];
        if (note >= 0 && note < 128) {
            int16_t transposedNote = (int16_t)note + transpose + cur->transpose[step];
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
                MIDI.sendNoteOn((uint8_t)transposedNote, cur->velocities[step][i], params.channel);
            }
            // Длина ноты — от этого шага: GATE паттерна, записанная длина, цепочка Tie
            uint16_t len = MelodicSequencer::computeNoteLengthTicks(
                step, i, cur->length, patternStepTicks(steps[currentStep].divider), cur->gate,
                cur->tie, &cur->lengthTicks[0][0]);
            
            // Добавляем в список играющих нот (избегаем дубликатов)
            bool duplicate = false;
            for (int j = 0; j < lastPlayedCountForTie; j++) {
                if (lastPlayedNotesForTie[j] == (uint8_t)transposedNote) {
                    duplicate = true;
                    lastPlayedTicksLeft[j] = len;
                    break;
                }
            }
            if (!duplicate && lastPlayedCountForTie < SONG_MAX_SOUNDING) {
                lastPlayedTicksLeft[lastPlayedCountForTie] = len;
                lastPlayedNotesForTie[lastPlayedCountForTie++] = (uint8_t)transposedNote;
            }
        }
    }
    
    lastPatternStep = step;
}


void SongSequencer::stopAllNotes() {
    // Гасим только свои звучащие ноты (раньше — NoteOff на все 128 нот + CC 123: ~120 мс,
    // на которые останавливались такты, и обрыв чужих нот на том же канале).
    for (int i = 0; i < lastPlayedCountForTie; i++) {
        MIDI.sendNoteOff(lastPlayedNotesForTie[i], 0, params.channel);
    }
    lastPlayedCountForTie = 0;
    lastPatternStep = 255;
}

void SongSequencer::play() {
    if (enabled) return;
    enabled = true;
    resetClockPhase();
    direction = 1;
    currentStep = startStep();  // REV — с последнего шага, как секвенсор
    stepsPlayed = 0;
    patternPlayStep = 0;
    patternPlayLength = 0;
    // Буферы паттернов не сбрасываем: пересохранённый паттерн сбрасывается в patternChanged(),
    // а паттерн первого шага уже подгружен в остановке (update)
    lastPatternStep = 255;
    lastPatternSlotForTie = 255;
    lastPlayedCountForTie = 0;
    startPending = true;  // шаг 1 начнётся на первом же такте
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void SongSequencer::stop() {
    if (!enabled) return;
    enabled = false;
    stopAllNotes();
    resetClockPhase();
    extern void ui_markDirty(uint8_t flags);
    ui_markDirty(1);
}

void SongSequencer::toggle() {
    if (enabled) stop();
    else play();
}

void SongSequencer::tap() {
    clock_tap();
}

void SongSequencer::clear() {
    uint8_t keepSong = currentSong;  // очистка не меняет номер слота песни
    initArrays();
    currentSong = keepSong;
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
    
    currentSong = slot;
    songDirty = false;
    return true;
}