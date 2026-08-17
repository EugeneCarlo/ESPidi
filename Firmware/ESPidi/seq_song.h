#ifndef SEQ_SONG_H
#define SEQ_SONG_H

#include <Arduino.h>
#include "config.h"
#include "app.h"
#include "seq_mel.h"

#define MAX_SONG_STEPS 64
#define MAX_SONGS 64
#define SONG_DIR "/songs"

struct SongStepParams {
    uint8_t patternSlot = 0;    // 0 = пауза, 1-64 = паттерн
    int8_t transpose = 0;       // -24..+24
    uint8_t divider = 4;        // 0-5, как в арпеджиаторе (по умолчанию 1/16)
    uint8_t pauseLength = 1;    // для пауз: длительность в шагах паттерна (1-64)
    bool mute = false;          // MUTE шага
};

struct SongParams {
    uint8_t bpm = 120;
    uint8_t channel = 1;
    uint8_t mode = 0;           // 0=FWD, 1=REV, 2=PEND, 3=RND
    uint8_t cycle = 0;          // 0=OFF, 1=ON
    uint8_t length = 16;        // длина песни в шагах
    uint8_t page = 1;           // текущая страница для UI
};

class SongSequencer : public App {
public:
    SongParams params;
    
    void begin() override;
    void update() override;
    void onClockTick() override;
    void resetClockPhase() override;
    
    bool handleNoteOn(uint8_t note, uint8_t velocity) override { return false; }
    bool handleNoteOff(uint8_t note) override { return false; }
    bool handleCC(uint8_t number, uint8_t value) override { return false; }
    bool isEnabled() override { return enabled; }
    
    void play() override;
    void stop() override;
    void toggle() override;
    void tap() override;
    
    void clear();
    void clearStep(uint8_t step);
    bool saveToFile(uint8_t slot);
    bool loadFromFile(uint8_t slot);
    uint8_t getCurrentSong() const { return currentSong; }
    void markDirty() { songDirty = true; }
    bool isDirty() const { return songDirty; }
    void markClean() { songDirty = false; }
    
    uint8_t getCurrentStep() const { return currentStep; }
    void setCurrentStep(uint8_t step) { currentStep = step % MAX_SONG_STEPS; }
    
    uint8_t getCurrentPage() const { return params.page; }
    bool getHasPattern(uint8_t step) const { return steps[step].patternSlot > 0; }
    bool getIsPause(uint8_t step) const { return steps[step].patternSlot == 0; }
    
    uint8_t getEditStep() const { return editStep; }
    void setEditStepDirect(uint8_t step) { editStep = step; }
    
    SongStepParams& getStepParams(uint8_t step) { return steps[step]; }
    uint8_t getPatternSlot(uint8_t step) const { return steps[step].patternSlot; }
    int8_t getTranspose(uint8_t step) const { return steps[step].transpose; }
    uint8_t getDivider(uint8_t step) const { return steps[step].divider; }
    uint8_t getPauseLength(uint8_t step) const { return steps[step].pauseLength; }
    
    void setPatternSlot(uint8_t step, uint8_t slot);
    void setTranspose(uint8_t step, int8_t value);
    void adjustTranspose(uint8_t step, int8_t delta);
    void setDivider(uint8_t step, uint8_t value);
    void setPauseLength(uint8_t step, uint8_t value);
    void toggleMute(uint8_t step);
    bool getMute(uint8_t step) const { return steps[step].mute; }
    
    bool stepEditActive = false;
    bool enabled = false;
    bool stepSelectMode = false;  // false = редактирование параметров, true = выбор шага
    
    // Режимы редактирования в STEP EDIT (циклическое переключение)
    enum StepEditParam {
        STEP_PARAM_PATTERN = 0,
        STEP_PARAM_TRANSPOSE = 1,
        STEP_PARAM_DIVIDER = 2,
        STEP_PARAM_PAUSE_LEN = 3,
        STEP_PARAM_COUNT = 4
    };
    uint8_t getStepEditParam() const { return stepEditParam; }
    void setStepEditParam(uint8_t p) { stepEditParam = p; }
    
private:
    uint8_t stepEditParam = STEP_PARAM_PATTERN;
    SongStepParams steps[MAX_SONG_STEPS];
    
    uint8_t currentStep = 0;
    uint8_t editStep = 0;
    int8_t direction = 1;
    uint16_t ticksIntoStep = 0;
    unsigned long lastTapTime = 0;
    uint8_t currentSong = 0;
    bool songDirty = false;
    
    // Буфер текущего играемого паттерна
    int8_t patternBuffer_notes[MAX_SEQ_STEPS][MAX_POLY];
    uint8_t patternBuffer_velocities[MAX_SEQ_STEPS][MAX_POLY];
    uint8_t patternBuffer_noteCount[MAX_SEQ_STEPS];
    uint8_t patternBuffer_ccNumber[MAX_SEQ_STEPS][MAX_CC_PER_STEP];
    uint8_t patternBuffer_ccValue[MAX_SEQ_STEPS][MAX_CC_PER_STEP];
    uint8_t patternBuffer_ccCount[MAX_SEQ_STEPS];
    bool patternBuffer_tie[MAX_SEQ_STEPS];
    int8_t patternBuffer_transpose[MAX_SEQ_STEPS];
    uint8_t patternBufferLength = 0;
    uint8_t patternBufferSlot = 255;  // 255 = буфер пуст
    bool loadPatternToBuffer(uint8_t slot);
    
    // Для отслеживания воспроизведения текущего паттерна
    uint8_t patternPlayStep = 0;      // текущий шаг внутри проигрываемого паттерна
    uint8_t patternPlayLength = 0;    // длина проигрываемого паттерна
    uint8_t currentPatternSlot = 0;   // слот текущего проигрываемого паттерна
    
    // Для отслеживания Tie между шагами паттерна
    uint8_t lastPatternStep = 255;
    uint8_t lastPatternSlotForTie = 255;
    int8_t lastPlayedNotesForTie[MAX_POLY];
    uint8_t lastPlayedCountForTie = 0;
    
    void initArrays();
    void stopAllNotes();
    void playPatternStep(uint8_t step, int8_t transpose);
    uint16_t patternStepTicks(uint8_t divider) const;
    void advanceSongStep();
    void doPatternPulse();
};

#endif