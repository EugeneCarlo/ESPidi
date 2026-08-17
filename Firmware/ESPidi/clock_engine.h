#ifndef CLOCK_ENGINE_H
#define CLOCK_ENGINE_H

#include <Arduino.h>

// MIDI Clock = 24 PPQN. quarter=24, 1/16=6, 1/32=3
extern uint8_t globalBpm;

void clock_begin();
void clock_update();

void clock_onMidiClock();
void clock_onMidiStart();
void clock_onMidiStop();
void clock_onMidiContinue();

void clock_setSourceExternal(bool on);
void clock_setOutEnabled(bool on);
void clock_setTransportEnabled(bool on);

bool clock_isSourceExternal();
bool clock_isOutEnabled();
bool clock_isTransportEnabled();
bool clock_isAlive();

uint8_t clock_getBpm();
void clock_setBpm(uint8_t bpm);
uint8_t clock_getDisplayBpm();

void clock_onLocalPlay(bool nowPlaying);

// division: 0=1/1 … 5=1/32 → тики при 24 PPQN
uint8_t clock_ticksPerDivision(uint8_t division);
uint32_t clock_getTickCount();
void clock_dispatchTick();

#endif
