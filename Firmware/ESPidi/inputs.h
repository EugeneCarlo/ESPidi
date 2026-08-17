#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>
#include "config.h"

extern volatile int encDelta;

void inputs_init();
void inputs_pollEncoderFast();
void inputs_pollButtons();


#endif