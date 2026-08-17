// midi_monitor.cpp
#include "midi_monitor.h"
#include "midi_handler.h"

void MidiMonitor::begin() {
    activeNoteCount = 0;
    activeCCCount = 0;
    lastChannel = 0;
    
    for (int i = 0; i < MONITOR_MAX_NOTES; i++) {
        notes[i].active = false;
        notes[i].velocity = 0;
        notes[i].timestamp = 0;
        notes[i].noteOffTime = 0;
    }
    
    for (int i = 0; i < MONITOR_MAX_CC; i++) {
        ccs[i].number = 0;
        ccs[i].value = 0;
        ccs[i].timestamp = 0;
        ccs[i].dirty = false;
    }
}

void MidiMonitor::update() {
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 500) {
        cleanupExpired();
        lastCleanup = millis();
    }
    
    unsigned long now = millis();
    for (int i = 0; i < activeCCCount; i++) {
        if (ccs[i].dirty && (now - ccs[i].timestamp > 1000)) {
            ccs[i].dirty = false;
        }
    }
}

bool MidiMonitor::handleNoteOn(uint8_t note, uint8_t velocity) {
    if (velocity == 0) {
        removeNote(note);
        return false;
    }
    
    addNote(lastChannel, note, velocity);
    return false;
}

bool MidiMonitor::handleNoteOff(uint8_t note) {
    removeNote(note);
    return false;
}

bool MidiMonitor::handleCC(uint8_t number, uint8_t value) {
    addOrUpdateCC(number, value);
    sortCCs();
    return false;
}

void MidiMonitor::addNote(uint8_t channel, uint8_t note, uint8_t velocity) {
    // Удаляем если такая же нота уже есть (дубликат)
    for (int i = 0; i < activeNoteCount; i++) {
        if (notes[i].note == note && notes[i].active) {
            notes[i].active = false;
            notes[i].noteOffTime = millis();
        }
    }
    
    // Если нет места — вытесняем
    if (activeNoteCount >= MONITOR_MAX_NOTES) {
        // Сначала пытаемся удалить неактивную
        int oldestOff = -1;
        unsigned long oldestTime = UINT32_MAX;
        
        for (int i = 0; i < activeNoteCount; i++) {
            if (!notes[i].active && notes[i].noteOffTime < oldestTime) {
                oldestTime = notes[i].noteOffTime;
                oldestOff = i;
            }
        }
        
        if (oldestOff >= 0) {
            for (int j = oldestOff; j < activeNoteCount - 1; j++) {
                notes[j] = notes[j + 1];
            }
            activeNoteCount--;
        } else {
            // Все активны — вытесняем самую старую
            unsigned long oldestActive = UINT32_MAX;
            int oldestIdx = 0;
            for (int i = 0; i < activeNoteCount; i++) {
                if (notes[i].timestamp < oldestActive) {
                    oldestActive = notes[i].timestamp;
                    oldestIdx = i;
                }
            }
            for (int j = oldestIdx; j < activeNoteCount - 1; j++) {
                notes[j] = notes[j + 1];
            }
            activeNoteCount--;
        }
    }
    
    notes[activeNoteCount].channel = channel;
    notes[activeNoteCount].note = note;
    notes[activeNoteCount].velocity = velocity;
    notes[activeNoteCount].active = true;
    notes[activeNoteCount].timestamp = millis();
    notes[activeNoteCount].noteOffTime = 0;
    activeNoteCount++;
    
    sortNotes();
}

void MidiMonitor::removeNote(uint8_t note) {
    for (int i = 0; i < activeNoteCount; i++) {
        if (notes[i].note == note && notes[i].active) {
            notes[i].active = false;
            notes[i].noteOffTime = millis();
            return;
        }
    }
}

void MidiMonitor::addOrUpdateCC(uint8_t number, uint8_t value) {
    for (int i = 0; i < activeCCCount; i++) {
        if (ccs[i].number == number) {
            if (ccs[i].value != value) {
                ccs[i].value = value;
                ccs[i].timestamp = millis();
                ccs[i].dirty = true;
            }
            return;
        }
    }
    
    if (activeCCCount >= MONITOR_MAX_CC) {
        unsigned long oldestTime = UINT32_MAX;
        int oldestIdx = 0;
        for (int i = 0; i < activeCCCount; i++) {
            if (ccs[i].timestamp < oldestTime) {
                oldestTime = ccs[i].timestamp;
                oldestIdx = i;
            }
        }
        for (int j = oldestIdx; j < activeCCCount - 1; j++) {
            ccs[j] = ccs[j + 1];
        }
        activeCCCount--;
    }
    
    ccs[activeCCCount].number = number;
    ccs[activeCCCount].value = value;
    ccs[activeCCCount].timestamp = millis();
    ccs[activeCCCount].dirty = true;
    activeCCCount++;
}

void MidiMonitor::cleanupExpired() {
    unsigned long now = millis();
    
    // Очистка NoteOff нот
    int writePos = 0;
    for (int i = 0; i < activeNoteCount; i++) {
        if (notes[i].active || (now - notes[i].noteOffTime < MONITOR_NOTE_TIMEOUT_MS)) {
            if (writePos != i) {
                notes[writePos] = notes[i];
            }
            writePos++;
        }
    }
    activeNoteCount = writePos;
    
    // Очистка старых CC
    writePos = 0;
    for (int i = 0; i < activeCCCount; i++) {
        if (now - ccs[i].timestamp < MONITOR_CC_TIMEOUT_MS) {
            if (writePos != i) {
                ccs[writePos] = ccs[i];
            }
            writePos++;
        }
    }
    activeCCCount = writePos;
}

void MidiMonitor::sortNotes() {
    // Сортировка: сначала активные (по высоте тона), потом неактивные (по времени noteOff — чем свежее, тем выше)
    for (int i = 0; i < activeNoteCount - 1; i++) {
        for (int j = 0; j < activeNoteCount - i - 1; j++) {
            bool swap = false;
            
            // Активные всегда перед неактивными
            if (!notes[j].active && notes[j + 1].active) {
                swap = true;
            }
            // Обе активны — сортировка по высоте
            else if (notes[j].active && notes[j + 1].active && notes[j].note > notes[j + 1].note) {
                swap = true;
            }
            // Обе неактивны — кто позже отпущен, тот выше (новее)
            else if (!notes[j].active && !notes[j + 1].active && notes[j].noteOffTime < notes[j + 1].noteOffTime) {
                swap = true;
            }
            
            if (swap) {
                NoteEvent temp = notes[j];
                notes[j] = notes[j + 1];
                notes[j + 1] = temp;
            }
        }
    }
}

void MidiMonitor::sortCCs() {
    for (int i = 0; i < activeCCCount - 1; i++) {
        for (int j = 0; j < activeCCCount - i - 1; j++) {
            if (ccs[j].number > ccs[j + 1].number) {
                CCEvent temp = ccs[j];
                ccs[j] = ccs[j + 1];
                ccs[j + 1] = temp;
            }
        }
    }
}

void MidiMonitor::toggleNoteDisplay() {
    displayNoteNames = !displayNoteNames;
}