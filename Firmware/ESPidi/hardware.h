#ifndef HARDWARE_H
#define HARDWARE_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MIDI.h>
#include "config.h"
#include "midi_serial.h"

extern Adafruit_SSD1306 display;
extern uint8_t oledAddr;
extern MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<MidiSerial>> MIDI;
extern MidiSerial midiSerial;

void hw_initDisplay();
void hw_initMIDI();
void hw_initPins();

#endif