#include "inputs.h"
#include "ui.h"

volatile int encDelta = 0;
static volatile int8_t encAccum = 0;
static volatile uint8_t encLastState = 0;

static unsigned long btnDebounce[4] = {0};
static unsigned long btnPressStart[4] = {0};
static bool btnLongDone[4] = {false};
extern bool tapUsedInCombo;

// Gray-code LUT: (prev<<2)|curr → delta
static const int8_t ENC_TABLE[] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

void IRAM_ATTR encIsr() {
    uint8_t state = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
    int8_t movement = ENC_TABLE[(encLastState << 2) | state];
    encLastState = state;
    if (movement == 0) return;

    encAccum += movement;
    if (encAccum >= 4) {
        encAccum -= 4;
        encDelta++;
    } else if (encAccum <= -4) {
        encAccum += 4;
        encDelta--;
    }
}

void inputs_init() {
    encLastState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
    encAccum = 0;
    encDelta = 0;
    attachInterrupt(digitalPinToInterrupt(ENC_A), encIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encIsr, CHANGE);
}

// Fallback polling (дублирует ISR безопасно: если IRQ пропустил — догоним)
int8_t readEncoder() {
    static uint8_t lastState = 0;
    static int8_t accum = 0;
    static bool inited = false;

    uint8_t currentState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
    if (!inited) {
        lastState = currentState;
        inited = true;
        return 0;
    }
    if (currentState == lastState) return 0;

    int8_t movement = ENC_TABLE[(lastState << 2) | currentState];
    lastState = currentState;
    if (movement == 0) return 0;

    accum += movement;
    if (accum >= 4) { accum = 0; return +1; }
    if (accum <= -4) { accum = 0; return -1; }
    return 0;
}

void inputs_pollEncoderFast() {
    // ISR уже копит encDelta; polling — запасной путь без двойного счёта:
    // не используем readEncoder параллельно с ISR (будет double-count).
    // Оставляем пустым / только drain barrier.
}

void inputs_pollButtons() {
    static unsigned long lastButtons = 0;
    if (millis() - lastButtons < 5) return;
    lastButtons = millis();
    
    unsigned long now = millis();
    uint8_t buttonPins[] = {ENC_BTN, BTN_L_R, BTN_TAP, BTN_PLAY};
    
    for (int i = 0; i < 4; i++) {
        bool reading = (digitalRead(buttonPins[i]) == LOW);
        
        if (reading && btnDebounce[i] == 0) {
            btnDebounce[i] = now;
            btnPressStart[i] = now;
            btnLongDone[i] = false;
        }
        
        if (reading && btnDebounce[i] > 0 && (now - btnDebounce[i] > DEBOUNCE_MS)) {
            if (!btnLongDone[i] && (now - btnPressStart[i] > LONG_PRESS_MS)) {
                btnLongDone[i] = true;
                if (i == 0) ui_handleEncoderPress(false);
                else ui_handleButton(buttonPins[i], true);
            }
        }
        
        if (!reading && btnDebounce[i] > 0 && (now - btnDebounce[i] > DEBOUNCE_MS)) {
            if ((now - btnPressStart[i]) < LONG_PRESS_MS) {
                if (i == 0) ui_handleEncoderPress(true);
                else ui_handleButton(buttonPins[i], false);
            }
            btnDebounce[i] = 0;
            if (buttonPins[i] == BTN_TAP) tapUsedInCombo = false;
        }
    }
}
