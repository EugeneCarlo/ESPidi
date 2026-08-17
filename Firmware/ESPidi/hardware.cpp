#include "hardware.h"

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

void hw_initDisplay() {
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 5);
    display.println("MIDIPAL");
    display.display();
    delay(800);
}

void hw_initMIDI() {
    Serial1.begin(31250, SERIAL_8N1, MIDI_RX, MIDI_TX);
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();
}

void hw_initPins() {
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    pinMode(ENC_BTN, INPUT_PULLUP);
    pinMode(BTN_L_R, INPUT_PULLUP);
    pinMode(BTN_TAP, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
}