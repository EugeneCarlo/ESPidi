#ifndef SEQ_MEL_H
#define SEQ_MEL_H

#include <Arduino.h>
#include "config.h"
#include "app.h"
#include "arp.h"

#define MAX_SEQ_STEPS 64
#define MAX_POLY 6
#define MAX_CC_PER_STEP 3
#define STEPS_PER_PAGE 16
#define MAX_ACTIVE_SEQ_NOTES 24
#define PATTERN_FILE_VERSION 2

// Смена PTRN во время игры (SettingsParams::ptrnSwitch)
#define PTRN_SWITCH_NOW  0  // сразу: шаг нового паттерна звучит в момент смены, следующий — по сетке
#define PTRN_SWITCH_NEXT 1  // со следующего шага
#define PTRN_SWITCH_END  2  // после последнего шага текущего паттерна, новый — с начала

struct MelSeqParams {
    uint8_t bpm = 120;
    uint8_t channel = 1;
    uint8_t mode = 0;
    uint8_t length = 16;
    uint8_t gate = 80;
    uint8_t page = 1;
    uint8_t reRec = 1;
    uint8_t strum = 0;
    uint8_t follow = 1;
    uint8_t swing = 0;
    uint8_t randomness = 0;
    uint8_t probability = 0;
};

struct SeqActiveNote {
    uint8_t note = 0;
    uint8_t channel = 1;
    uint8_t velocity = 0;
    uint16_t ticksLeft = 0;
    bool used = false;
};

struct MelPatternFile;  // содержимое файла паттерна (seq_mel.cpp)

class MelodicSequencer : public App {
public:
    MelSeqParams params;
    
    void begin() override;
    void update() override;
    void onClockTick() override;
    void resetClockPhase() override;
    
    bool handleNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) override;
    bool handleNoteOff(uint8_t note, uint8_t channel) override;
    bool handleCC(uint8_t number, uint8_t value, uint8_t channel) override;
    bool isEnabled() override { return enabled; }
    bool passesThru() override { return params.strum == 1; }  // THRU
    
    void play() override;
    void stop() override;
    void toggle() override;
    void tap() override;
    
    void clear();
    void toggleRecord();
    void clearStep(uint8_t step);
    bool saveToFile(uint8_t slot);
    bool loadFromFile(uint8_t slot);
    // Смена паттерна из меню: файл читается в основном цикле вне блокировки движка
    // (loadRequestedPattern), применяется под ней — такты не ждут чтения LittleFS.
    void requestPattern(uint8_t slot) { requestedPattern = slot; }
    void loadRequestedPattern();
    bool isSwitchPending() const { return switchPending; }  // новый паттерн прочитан и ждёт своего шага
    uint8_t getCurrentPattern() const { return currentPattern; }
    void markDirty() { patternDirty = true; }
    bool isDirty() const { return patternDirty; }
    void markClean() { patternDirty = false; }
    
    uint8_t getCurrentStep() const { return currentStep; }
    uint8_t getLastPlayedStep() const { return lastPlayedStep; }
    void setCurrentStep(uint8_t step) { currentStep = step % MAX_SEQ_STEPS; }
    
    uint8_t getCurrentPage() const { return params.page; }
    bool getHasNote(uint8_t step) const;
    bool getHasCC(uint8_t step) const { return ccCount[step] > 0; }
    bool getTie(uint8_t step) const { return tie[step]; }
    void setTie(uint8_t step, bool value) { tie[step] = value; patternDirty = true; }
    void toggleTie(uint8_t step);
    uint8_t getNoteCount(uint8_t step) const { return getDisplayNoteCount(step); }
    uint8_t getCCCount(uint8_t step) const { return ccCount[step]; }
    uint8_t getEditStep() const { return editStep; }
    void setEditStepDirect(uint8_t step) { editStep = step; }
    bool stepEditActive = false;
    bool transposeEditActive = false;
    bool enabled = false;
    uint8_t recording = 0;
    
    int8_t getNoteAt(uint8_t step, uint8_t index) const;
    uint8_t getVelocityAt(uint8_t step, uint8_t index) const;
    uint8_t getCCNumberAt(uint8_t step, uint8_t index) const;
    uint8_t getCCValueAt(uint8_t step, uint8_t index) const;
    int8_t getTranspose(uint8_t step) const;
    void setTranspose(uint8_t step, int8_t value);
    void adjustTranspose(uint8_t step, int8_t delta);
    
    bool getStepData(uint8_t patternSlot, uint8_t step,
        int8_t* outNotes, uint8_t* outVelocities, uint8_t& outNoteCount,
        uint8_t* outCCNum, uint8_t* outCCVal, uint8_t& outCCCount,
        bool& outTie, int8_t& outTranspose, uint8_t& outLength);
    
    // Общие helpers для song mode (pattern buffer playback)
    static uint16_t computeNoteLengthTicks(
        uint8_t startStep, uint8_t noteIndex, uint8_t patternLength,
        uint16_t stepLenTicks, uint8_t gate,
        const bool* tieArr, const uint8_t* lenArr);
    static void activePoolTick(SeqActiveNote* pool, int maxPool);
    static void activePoolStopAll(SeqActiveNote* pool, int maxPool);
    static void activePoolNoteOn(SeqActiveNote* pool, int maxPool,
        uint8_t note, uint8_t channel, uint8_t velocity, uint16_t ticks);
    
private:
    int8_t notes[MAX_SEQ_STEPS][MAX_POLY];
    uint8_t velocities[MAX_SEQ_STEPS][MAX_POLY];
    uint8_t channels[MAX_SEQ_STEPS][MAX_POLY];
    uint8_t lengthTicks[MAX_SEQ_STEPS][MAX_POLY];  // 0 = авто из GATE
    uint8_t noteCount[MAX_SEQ_STEPS];
    bool tie[MAX_SEQ_STEPS];
    int8_t transpose[MAX_SEQ_STEPS];
    
    uint8_t ccNumber[MAX_SEQ_STEPS][MAX_CC_PER_STEP];
    uint8_t ccValue[MAX_SEQ_STEPS][MAX_CC_PER_STEP];
    uint8_t ccCount[MAX_SEQ_STEPS];
    
    uint8_t currentStep = 0;
    uint8_t editStep = 0;
    int8_t direction = 1;
    uint16_t ticksIntoStep = 0;
    
    SeqActiveNote activeNotes[MAX_ACTIVE_SEQ_NOTES];
    
    unsigned long lastChordTime = 0;
    bool noteHeld = false;
    uint8_t lastPlayedStep = 0;   // шаг, который звучит сейчас (последний сыгранный)
    uint8_t heldStep = 0;         // шаг, куда записана удерживаемая нота (для авто-Tie)
    uint8_t currentPattern = 0;
    bool patternDirty = false;
    uint8_t requestedPattern = 255;  // 255 — запроса нет
    bool switchPending = false;      // прочитанный паттерн ждёт границы шага (NEXT) или конца (END)
    uint8_t pendingSlot = 0;
    int pendingStatus = 0;
    bool freshStart = false;         // шаг сразу после смены паттерна: Tie из старого не тянется
    uint32_t stepsSincePlay = 0;     // шагов сыграно с PLAY — по нему позиция нового паттерна
    uint32_t patternSteps = 0;       // шагов сыграно текущим паттерном (конец паттерна в RND)
    
    void initArrays();
    static int readPatternFile(uint8_t slot, MelPatternFile& f);
    bool applyPatternFile(uint8_t slot, int status, const MelPatternFile& f, bool keepTransport = false);
    uint8_t positionFor(uint32_t globalStep, int8_t& dir) const;
    bool atPatternStart() const;
    void applyPendingSwitch();
    void switchNow(uint8_t slot, int status, const MelPatternFile& f);
    void stopAllNotes();
    void playStep(uint8_t step, bool doRandomize);
    uint16_t stepTicks() const;
    void doStep();
    uint8_t getDisplayNoteCount(uint8_t step) const;
    int findStoredNoteIndex(uint8_t step, uint8_t displayIndex) const;
    uint16_t defaultGateTicks() const;
    uint16_t noteLengthFor(uint8_t step, uint8_t index) const;
    void extendActiveNotesThroughTie(uint8_t step);
    uint8_t recordTargetStep() const;
    void recordNote(uint8_t note, uint8_t velocity, uint8_t channel);
    void recordCC(uint8_t number, uint8_t value);
};

#endif
