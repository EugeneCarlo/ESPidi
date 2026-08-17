#ifndef UI_H
#define UI_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "app.h"
#include "arp.h"
#include "seq_song.h"

struct ParamDef {
    const char* label;
    uint8_t* value;
    uint8_t min, max;
    const char** names;
    uint8_t namesCount;
};

enum UIState { UI_NAVIGATE, UI_EDIT, UI_MENU, UI_STEP_EDIT, UI_HELP };
enum Column { COL_LEFT, COL_RIGHT };

extern UIState uiState;
extern Column currentCol;
extern int leftScrollOffset;
extern int rightScrollOffset;
extern int cursorVisualRow;
extern int leftParamCount;
extern int rightParamCount;
extern ParamDef* leftParams;
extern ParamDef* rightParams;
extern AppType currentAppType;
extern int menuPosition;  // Добавлено

#define UI_DIRTY_FULL     1
#define UI_DIRTY_PLAYHEAD 2
#define UI_DIRTY_BPM      4

void ui_init();
void ui_setApp(AppType app);
void ui_handleEncoder(int delta);
void ui_handleEncoderPress(bool shortPress);
void ui_handleButton(uint8_t pin, bool longPress);
void ui_drawScreen();
void ui_markDirty(uint8_t flags = UI_DIRTY_FULL);

String getNoteName(int8_t note);

#endif