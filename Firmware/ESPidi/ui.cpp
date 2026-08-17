#include "ui.h"
#include "hardware.h"
#include "arp.h"
#include "seq_mel.h"
#include "inputs.h"
#include "midi_handler.h"
#include "midi_monitor.h"
#include "clock_engine.h"
#include "settings.h"

extern Arpeggiator arp;
extern MelodicSequencer melSeq;
extern SongSequencer songSeq;

static uint8_t uiDirty = UI_DIRTY_FULL;

void ui_markDirty(uint8_t flags) {
  uiDirty |= flags;
}

// Сокращения для StepEditParam
#define STEP_PARAM_PATTERN   SongSequencer::STEP_PARAM_PATTERN
#define STEP_PARAM_TRANSPOSE SongSequencer::STEP_PARAM_TRANSPOSE
#define STEP_PARAM_DIVIDER   SongSequencer::STEP_PARAM_DIVIDER
#define STEP_PARAM_PAUSE_LEN SongSequencer::STEP_PARAM_PAUSE_LEN
#define STEP_PARAM_COUNT     SongSequencer::STEP_PARAM_COUNT

const char* arpModeNames[] = { "UP", "DN", "UD", "RND" };
const char* divNames[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
const char* onOffNames[] = { "OFF", "ON" };
const char* seqModeNames[] = { "FWD", "REV", "PEND", "RND" };
const char* cycleNames[] = { "OFF", "ON" };
const char* pageNames[] = { "1", "2", "3", "4" };

ParamDef arpLeft[] = {
  { "BPM", &globalBpm, 40, 250, nullptr, 0 },
  { "MODE", &arp.params.mode, 0, 3, arpModeNames, 4 },
  { "DIV", &arp.params.division, 0, 5, divNames, 6 },
  { "STRUM", &arp.params.strum, 0, 1, onOffNames, 2 },
  { "HOLD", &arp.params.hold, 0, 1, onOffNames, 2 }
};

ParamDef arpRight[] = {
  { "CH", &arp.params.channel, 1, 16, nullptr, 0 },
  { "OCT", &arp.params.octaves, 1, 4, nullptr, 0 },
  { "GATE", &arp.params.gate, 0, 127, nullptr, 0 },
  { "SWING", &arp.params.swing, 0, 127, nullptr, 0 }
};

ParamDef seqLeft[] = {
  { "BPM", &globalBpm, 40, 250, nullptr, 0 },
  { "MODE", &melSeq.params.mode, 0, 3, seqModeNames, 4 },
  { "LENGTH", &melSeq.params.length, 1, 64, nullptr, 0 },
  { "STRUM", &melSeq.params.strum, 0, 1, onOffNames, 2 },
  { "RE-REC", &melSeq.params.reRec, 0, 1, onOffNames, 2 }
};

static uint8_t patternSlot = 0;
static uint8_t songSlot = 0;
static uint8_t saveTrigger = 0;  // фиктивное значение для параметра SAVE

ParamDef seqRight[] = {
  { "CH", &melSeq.params.channel, 1, 16, nullptr, 0 },
  { "PTRN", &patternSlot, 1, 64, nullptr, 0 },
  { "SAVE", &saveTrigger, 0, 0, nullptr, 0 },
  { "GATE", &melSeq.params.gate, 0, 127, nullptr, 0 },
  { "SWING", &melSeq.params.swing, 0, 127, nullptr, 0 },
  { "RAND", &melSeq.params.randomness, 0, 127, nullptr, 0 },
  { "PROB", &melSeq.params.probability, 0, 127, nullptr, 0 }
};

ParamDef songLeft[] = {
  { "BPM", &globalBpm, 40, 250, nullptr, 0 },
  { "MODE", &songSeq.params.mode, 0, 3, seqModeNames, 4 },
  { "LENGTH", &songSeq.params.length, 1, 64, nullptr, 0 },
  { "CYCLE", &songSeq.params.cycle, 0, 1, cycleNames, 2 }
};

ParamDef songRight[] = {
  { "CH", &songSeq.params.channel, 1, 16, nullptr, 0 },
  { "SONG", &songSlot, 1, 64, nullptr, 0 },
  { "SAVE", &saveTrigger, 0, 0, nullptr, 0 }
};

ParamDef settingsLeft[] = {
  { "CLKIN", &settingsApp.params.clockIn, 0, 1, onOffNames, 2 },
  { "CLKOUT", &settingsApp.params.clockOut, 0, 1, onOffNames, 2 },
  { "START", &settingsApp.params.start, 0, 1, onOffNames, 2 }
};

const char* brightNames[] = { "", "1", "2", "3", "4", "5", "6", "7", "8" };

ParamDef settingsRight[] = {
  { "BRIGHT", &settingsApp.params.brightness, 1, 8, brightNames, 9 },
  { "HELP", nullptr, 0, 0, nullptr, 0 }
};

ParamDef* leftParams = arpLeft;
ParamDef* rightParams = arpRight;

UIState uiState = UI_NAVIGATE;
Column currentCol = COL_LEFT;
int leftScrollOffset = 0;
int rightScrollOffset = 0;
int cursorVisualRow = 0;
int leftParamCount = sizeof(arpLeft) / sizeof(arpLeft[0]);
int rightParamCount = sizeof(arpRight) / sizeof(arpRight[0]);
AppType currentAppType = APP_ARPEGGIATOR;
int menuPosition = 0;
bool stepEditMode = false;
bool tapUsedInCombo = false;
uint8_t lastEditStep = 0;
bool lastEditStepValid = false;

String getNoteName(int8_t note) {
  if (note < 0 || note > 127) return "---";
  const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  String result = names[note % 12];
  result += (note / 12) - 1;
  return result;
}

void ui_setApp(AppType app) {
  currentAppType = app;
  stepEditMode = false;
  lastEditStepValid = false;

  if (app == APP_ARPEGGIATOR) {
    leftParams = arpLeft;
    rightParams = arpRight;
    leftParamCount = sizeof(arpLeft) / sizeof(arpLeft[0]);
    rightParamCount = sizeof(arpRight) / sizeof(arpRight[0]);
    currentApp = &arp;
  } else if (app == APP_MEL_SEQ) {
    leftParams = seqLeft;
    rightParams = seqRight;
    leftParamCount = sizeof(seqLeft) / sizeof(seqLeft[0]);
    rightParamCount = sizeof(seqRight) / sizeof(seqRight[0]);
    currentApp = &melSeq;
    patternSlot = melSeq.getCurrentPattern() + 1;
  } else if (app == APP_SONG) {
    leftParams = songLeft;
    rightParams = songRight;
    leftParamCount = sizeof(songLeft) / sizeof(songLeft[0]);
    rightParamCount = sizeof(songRight) / sizeof(songRight[0]);
    currentApp = &songSeq;
    songSlot = songSeq.getCurrentSong() + 1;
  } else if (app == APP_MONITOR) {
    leftParams = nullptr;
    rightParams = nullptr;
    leftParamCount = 0;
    rightParamCount = 0;
    currentApp = &monitor;
    monitor.begin();
  } else if (app == APP_SETTINGS) {
    leftParams = settingsLeft;
    rightParams = settingsRight;
    leftParamCount = sizeof(settingsLeft) / sizeof(settingsLeft[0]);
    rightParamCount = sizeof(settingsRight) / sizeof(settingsRight[0]);
    currentApp = &settingsApp;
    settingsApp.begin();
  }

  leftScrollOffset = 0;
  rightScrollOffset = 0;
  cursorVisualRow = 0;
  uiState = UI_NAVIGATE;
  currentCol = COL_LEFT;
  ui_markDirty(UI_DIRTY_FULL);
}

static void drawColumn(ParamDef* params, int count, int colX, bool isLeft) {
  int scrollOffset = isLeft ? leftScrollOffset : rightScrollOffset;

  for (int i = 0; i < VISIBLE_ROWS; i++) {
    int paramIndex = scrollOffset + i;
    if (paramIndex >= count) break;

    int y = i * 8;
    bool isActive = (isLeft ? currentCol == COL_LEFT : currentCol == COL_RIGHT) && (i == cursorVisualRow);
    bool isEditMode = isActive && uiState == UI_EDIT;
    bool isCursorMode = isActive && uiState == UI_NAVIGATE;
    bool isSaveParam = (strcmp(params[paramIndex].label, "SAVE") == 0);

    int labelEndX = colX + (strlen(params[paramIndex].label) + 1) * 6;

    if (isSaveParam && (isEditMode || isCursorMode)) {
      // SAVE с курсором: рисуем лейбл и значение вместе инвертированными
      display.setCursor(colX, y);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.print("SAVE:");
      bool ok = false;
      if (currentAppType == APP_MEL_SEQ) {
        ok = !melSeq.isDirty();
      } else if (currentAppType == APP_SONG) {
        ok = !songSeq.isDirty();
      }
      if (ok) {
        display.print("OK");
      }
    } else {
      display.setCursor(colX, y);
      display.setTextColor(isCursorMode ? SSD1306_BLACK : SSD1306_WHITE,
                           isCursorMode ? SSD1306_WHITE : SSD1306_BLACK);
      // При CLKIN лейбл темпа = EXT
      if (strcmp(params[paramIndex].label, "BPM") == 0 && clock_isSourceExternal()) {
        display.print("EXT");
      } else {
        display.print(params[paramIndex].label);
      }
      display.print(":");

      display.setCursor(labelEndX, y);
      if (isEditMode || isCursorMode) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      if (params[paramIndex].names) {
        uint8_t val = *params[paramIndex].value;
        display.print(params[paramIndex].names[val]);
      } else if (params[paramIndex].value == nullptr) {
        // Параметр без значения (например HELP) — ничего не выводим
      } else {
        if (isSaveParam) {
          bool ok = false;
          if (currentAppType == APP_MEL_SEQ) {
            ok = !melSeq.isDirty();
          } else if (currentAppType == APP_SONG) {
            ok = !songSeq.isDirty();
          }
          if (ok) {
            display.print("OK");
          }
        } else if (strcmp(params[paramIndex].label, "BPM") == 0 && clock_isSourceExternal()) {
          uint8_t d = clock_getDisplayBpm();
          if (d == 0) display.print("~");
          else display.print(d);
        } else {
          display.print(*params[paramIndex].value);
        }
      }
    }
  }
}

void ui_drawScreen() {
  // Нечего обновлять — не трогаем I2C (критично для MIDI clock)
  if (uiDirty == 0) {
    if (currentAppType == APP_MONITOR) {
      static unsigned long lastMon = 0;
      if (millis() - lastMon < 50) return;
      lastMon = millis();
      uiDirty = UI_DIRTY_FULL;
    } else {
      return;
    }
  }

  // Склеиваем частые playhead-обновления (флаги сохраняем при откладывании)
  static unsigned long lastDraw = 0;
  unsigned long now = millis();
  if (!(uiDirty & UI_DIRTY_FULL) && (now - lastDraw < 40)) {
    return;
  }
  lastDraw = now;
  uiDirty = 0;

  MIDI.read();

  display.clearDisplay();
  display.setTextSize(1);

  if (uiState == UI_MENU) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, 5);
    display.print(appNames[menuPosition]);
    display.display();
    MIDI.read();
    return;
  }
  // HELP экран для Settings
  if (uiState == UI_HELP) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("MIDIPAL v1.0");
    display.println("");
    display.println("TAP: set tempo");
    display.println("PLAY: start/stop");
    display.println("L_R: switch column");
    display.println("ENC: navigate/edit");
    display.println("TAP+L_R: step edit");
    display.println("TAP+PLAY: rec mode");
    display.println("LONG PLAY: clear");
    display.display();
    MIDI.read();
    return;
  }
  // STEP EDIT режим
  if (uiState == UI_STEP_EDIT && currentAppType == APP_MEL_SEQ) {
    uint8_t step = melSeq.getEditStep();

    // Левая колонка: ноты 1-3
    for (int i = 0; i < 3; i++) {
      int y = i * 8;
      int8_t note = melSeq.getNoteAt(step, i);
      display.setCursor(0, y);
      display.setTextColor(SSD1306_WHITE);
      if (note >= 0) {
        display.print(getNoteName(note));
      } else if (i < melSeq.getNoteCount(step)) {
        display.print("...");
      }
    }

    // Вторая колонка: ноты 4-6 (прижата к первой)
    for (int i = 3; i < 6; i++) {
      int y = (i - 3) * 8;
      int8_t note = melSeq.getNoteAt(step, i);
      display.setCursor(24, y);
      display.setTextColor(SSD1306_WHITE);
      if (note >= 0) {
        display.print(getNoteName(note));
      } else if (i < melSeq.getNoteCount(step)) {
        display.print("...");
      }
    }

    // Индикатор Tie
    if (melSeq.getTie(step)) {
      display.setCursor(50, 0);
      display.setTextColor(SSD1306_WHITE);
      display.print("*");
    }
    
    // Значение transpose под Tie
    // Знак на x=44, первая цифра на x=50, вторая (если есть) на x=56
    int8_t tr = melSeq.getTranspose(step);
    if (melSeq.transposeEditActive) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(44, 8);
    if (tr > 0) display.print("+");
    else if (tr < 0) display.print("-");
    display.setCursor(50, 8);
    if (abs(tr) >= 10) {
      display.print(abs(tr) / 10);
      display.setCursor(56, 8);
      display.print(abs(tr) % 10);
    } else {
      display.print(abs(tr));
    }

    // Правая колонка: CC (3 строки)
    for (int i = 0; i < 3; i++) {
      int y = i * 8;
      display.setCursor(64, y);
      display.setTextColor(SSD1306_WHITE);
      if (i < melSeq.getCCCount(step)) {
        display.print("CC");
        display.print(melSeq.getCCNumberAt(step, i));
        display.print(":");
        display.print(melSeq.getCCValueAt(step, i));
      }
    }

    // Play/Stop/REC иконка
    if (melSeq.recording) {
      display.fillCircle(123, 5, 3, SSD1306_WHITE);
    } else if (melSeq.enabled) {
      display.fillTriangle(120, 2, 120, 8, 126, 5, SSD1306_WHITE);
    } else {
      display.fillRect(120, 2, 6, 6, SSD1306_WHITE);
    }

    // Текущая страница STEP EDIT
    uint8_t editPage = (melSeq.getEditStep() / STEPS_PER_PAGE);

    // Номер страницы под иконкой (инвертированный)
    display.setTextSize(1);
    display.setCursor(120, 12);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(editPage + 1);
    display.setTextColor(SSD1306_WHITE);

    // Нижняя строка: шаги
    uint8_t startStep = editPage * STEPS_PER_PAGE;
    uint8_t playStep = melSeq.getCurrentStep();

    for (int i = 0; i < STEPS_PER_PAGE; i++) {
      uint8_t s = startStep + i;
      int x = i * 8;

      if (s >= melSeq.params.length) break;

      bool hasNote = melSeq.getHasNote(s);
      bool hasCC = melSeq.getHasCC(s);
      bool isCurrent = (s == step);
      bool isPlaying = (melSeq.enabled && s == playStep);

      // Сначала рисуем содержимое ячейки
      if (hasNote) {
        display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
      } else {
        display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
      }
      
      // Поверх рисуем индикаторы
      if (isPlaying && isCurrent) {
        // Играющий шаг и он же редактируемый — двойная рамка
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
        display.drawRect(x-2, 23, 11, 11, SSD1306_WHITE);
      } else if (isPlaying) {
        // Играющий шаг — рамка вокруг
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
      } else if (isCurrent) {
        // Редактируемый шаг — рамка чуть дальше
        display.drawRect(x-2, 23, 11, 11, SSD1306_WHITE);
      }

      if (hasCC) {
        display.setCursor(x + 1, 26);
        display.setTextSize(1);
        if (hasNote && !isCurrent) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else if (isCurrent) {
          display.setTextColor(SSD1306_WHITE);
        } else if (!hasNote) {
          display.setTextColor(SSD1306_WHITE);
        }
        display.print("~");
        display.setTextColor(SSD1306_WHITE);
      }
    }

    display.display();
    MIDI.read();
    return;
  }

  // STEP EDIT режим для SONG MODE
  if (stepEditMode && currentAppType == APP_SONG && (uiState == UI_STEP_EDIT || uiState == UI_NAVIGATE || uiState == UI_EDIT)) {
    uint8_t step = songSeq.getEditStep();
    SongStepParams& sp = songSeq.getStepParams(step);
    
    bool selectMode = songSeq.stepSelectMode;
    uint8_t activeParam = songSeq.getStepEditParam();
    
    // Для пауз пропускаем TRANS
    if (sp.patternSlot == 0 && activeParam == STEP_PARAM_TRANSPOSE) {
      activeParam = STEP_PARAM_PAUSE_LEN;
    }
    
    // === ЛЕВАЯ КОЛОНКА: параметры ===
    // PTRN
    bool isPTRN = (!selectMode && activeParam == STEP_PARAM_PATTERN);
    display.setCursor(0, 0);
    if (isPTRN && uiState == UI_NAVIGATE) {
      // Выбор параметра — инвертируется всё
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.print("PTRN:");
      if (sp.patternSlot == 0) display.print("---");
      else display.print(sp.patternSlot);
    } else {
      // Название всегда белое
      display.setTextColor(SSD1306_WHITE);
      display.print("PTRN:");
      // Значение инвертируется только в UI_EDIT
      if (isPTRN && uiState == UI_EDIT) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      if (sp.patternSlot == 0) display.print("---");
      else display.print(sp.patternSlot);
    }
    
    // TRANS или PAUSE
    bool isTransPause = (!selectMode && (activeParam == STEP_PARAM_TRANSPOSE || activeParam == STEP_PARAM_PAUSE_LEN));
    display.setCursor(0, 8);
    if (isTransPause && uiState == UI_NAVIGATE) {
      // Выбор параметра — инвертируется всё
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      if (sp.patternSlot == 0) {
        display.print("PAUSE:");
        display.print(sp.pauseLength);
      } else {
        display.print("TRANS:");
        if (sp.transpose > 0) display.print("+");
        else if (sp.transpose < 0) display.print("-");
        display.print(abs(sp.transpose));
      }
    } else {
      // Название всегда белое
      display.setTextColor(SSD1306_WHITE);
      if (sp.patternSlot == 0) {
        display.print("PAUSE:");
      } else {
        display.print("TRANS:");
      }
      // Значение инвертируется только в UI_EDIT
      if (isTransPause && uiState == UI_EDIT) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      if (sp.patternSlot == 0) {
        display.print(sp.pauseLength);
      } else {
        if (sp.transpose > 0) display.print("+");
        else if (sp.transpose < 0) display.print("-");
        display.print(abs(sp.transpose));
      }
    }
    
    // DIV
    bool isDiv = (!selectMode && activeParam == STEP_PARAM_DIVIDER);
    display.setCursor(0, 16);
    if (isDiv && uiState == UI_NAVIGATE) {
      // Выбор параметра — инвертируется всё
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.print("DIV:");
      display.print(divNames[sp.divider]);
    } else {
      // Название всегда белое
      display.setTextColor(SSD1306_WHITE);
      display.print("DIV:");
      // Значение инвертируется только в UI_EDIT
      if (isDiv && uiState == UI_EDIT) {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.print(divNames[sp.divider]);
    }
    
    // === ПРАВАЯ КОЛОНКА ===
    // SLOT
    display.setCursor(64, 0);
    if (selectMode) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.print("SLOT:");
    display.print(step + 1);
    
    // MUTE
    display.setCursor(64, 8);
    display.setTextColor(SSD1306_WHITE);
    display.print("MUTE:");
    if (sp.mute) {
      display.print("ON");
    } else {
      display.print("OFF");
    }
    
    // SONG
    display.setCursor(64, 16);
    display.print("SONG:");
    display.print(songSeq.getCurrentSong() + 1);
    
    // Иконка
    if (songSeq.enabled) {
      display.fillTriangle(120, 2, 120, 8, 126, 5, SSD1306_WHITE);
    } else {
      display.fillRect(120, 2, 6, 6, SSD1306_WHITE);
    }
    
    // Номер страницы
    uint8_t editPage = (songSeq.getEditStep() / STEPS_PER_PAGE);
    display.setTextSize(1);
    display.setCursor(120, 12);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(editPage + 1);
    display.setTextColor(SSD1306_WHITE);
    
    // Нижняя строка: шаги
    uint8_t startStep = editPage * STEPS_PER_PAGE;
    uint8_t playStep = songSeq.getCurrentStep();
    
    for (int i = 0; i < STEPS_PER_PAGE; i++) {
      uint8_t s = startStep + i;
      int x = i * 8;
      
      if (s >= songSeq.params.length) break;
      
      bool hasPattern = songSeq.getHasPattern(s);
      bool isMuted = songSeq.getMute(s);
      bool isCurrent = (s == step);
      bool isPlaying = (songSeq.enabled && s == playStep);
      
      if (hasPattern) {
        display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
      } else {
        display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
      }
      
      if (isPlaying && isCurrent) {
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
        display.drawRect(x-2, 23, 11, 11, SSD1306_WHITE);
      } else if (isPlaying) {
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
      } else if (isCurrent) {
        display.drawRect(x-2, 23, 11, 11, SSD1306_WHITE);
      }
      
      // MUTE индикатор
      if (isMuted) {
        display.setCursor(x + 1, 26);
        display.setTextSize(1);
        if (hasPattern && !isCurrent && !isPlaying) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.print("M");
        display.setTextColor(SSD1306_WHITE);
      }
    }
    
    display.display();
    MIDI.read();
    return;
  }

  // Обычный режим
  if (currentAppType != APP_MONITOR && currentAppType != APP_SETTINGS) {
    if (currentAppType == APP_MEL_SEQ && melSeq.recording) {
      display.fillCircle(123, 5, 3, SSD1306_WHITE);
    } else if (currentApp->isEnabled()) {
      display.fillTriangle(120, 2, 120, 8, 126, 5, SSD1306_WHITE);
    } else {
      display.fillRect(120, 2, 6, 6, SSD1306_WHITE);
    }
  }

  // Номер страницы под иконкой
  if (currentAppType == APP_MEL_SEQ) {
    uint8_t mainPage = (melSeq.getCurrentStep() / STEPS_PER_PAGE) + 1;
    display.setTextSize(1);
    display.setCursor(121, 12);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(mainPage);
    display.setTextColor(SSD1306_WHITE);
  } else if (currentAppType == APP_SONG) {
    uint8_t mainPage = (songSeq.getCurrentStep() / STEPS_PER_PAGE) + 1;
    display.setTextSize(1);
    display.setCursor(121, 12);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(mainPage);
    display.setTextColor(SSD1306_WHITE);
  } else if (currentAppType == APP_MONITOR) {
    // Нет номера страницы в мониторе
  }

  drawColumn(leftParams, leftParamCount, 0, true);
  drawColumn(rightParams, rightParamCount, 64, false);

  display.setCursor(0, 24);

  if (currentAppType == APP_ARPEGGIATOR) {
    const uint8_t* notes = arp.getHeldNotes();
    uint8_t count = arp.getHeldCount();

    if (arp.params.hold) {
      notes = arp.getHoldNotes();
      count = arp.getHoldNoteCount();
    }

    if (count > 0) {
      int maxChars = 0, showCount = 0;
      for (int i = 0; i < count; i++) {
        String name = getNoteName(notes[i]);
        maxChars += name.length() + 1;
        if (i == 0) maxChars--;
        if (maxChars > 21) break;
        showCount++;
      }

      for (int i = 0; i < showCount; i++) {
        String noteName = getNoteName(notes[i]);
        if (arp.isEnabled() && (notes[i] % 12) == arp.getPlayingNote()) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.print(noteName);
        if (i < showCount - 1) {
          display.setTextColor(SSD1306_WHITE);
          display.print(" ");
        }
      }
    }
  } else if (currentAppType == APP_MEL_SEQ) {
    uint8_t currentStep = melSeq.getCurrentStep();
    uint8_t page = currentStep / STEPS_PER_PAGE;
    uint8_t startStep = page * STEPS_PER_PAGE;

    for (int i = 0; i < STEPS_PER_PAGE; i++) {
      uint8_t step = startStep + i;
      int x = i * 8;

      if (step >= melSeq.params.length) break;

      bool hasNote = melSeq.getHasNote(step);
      bool hasCC = melSeq.getHasCC(step);
      bool isCurrent = (melSeq.enabled && step == currentStep);

      if (isCurrent) {
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
        if (hasNote) {
          display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
        } else {
          display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
        }
      } else if (hasNote) {
        display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
      } else {
        display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
      }

      if (hasCC) {
        display.setCursor(x + 1, 26);
        display.setTextSize(1);
        if (hasNote && !isCurrent) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else if (isCurrent) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.print("~");
        display.setTextColor(SSD1306_WHITE);
      }
    }
  } else if (currentAppType == APP_SONG) {
    uint8_t currentStep = songSeq.getCurrentStep();
    uint8_t page = currentStep / STEPS_PER_PAGE;
    uint8_t startStep = page * STEPS_PER_PAGE;

    for (int i = 0; i < STEPS_PER_PAGE; i++) {
      uint8_t step = startStep + i;
      int x = i * 8;

      if (step >= songSeq.params.length) break;

      bool hasPattern = songSeq.getHasPattern(step);
      bool isMuted = songSeq.getMute(step);
      bool isCurrent = (songSeq.enabled && step == currentStep);

      if (isCurrent) {
        display.drawRect(x-1, 24, 9, 9, SSD1306_WHITE);
        if (hasPattern) {
          display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
        } else {
          display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
        }
      } else if (hasPattern) {
        display.fillRect(x, 25, 7, 7, SSD1306_WHITE);
      } else {
        display.drawRect(x, 25, 7, 7, SSD1306_WHITE);
      }
      
      // MUTE индикатор
      if (isMuted) {
        display.setCursor(x + 1, 26);
        display.setTextSize(1);
        if (hasPattern && !isCurrent) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        display.print("M");
        display.setTextColor(SSD1306_WHITE);
      }
    }
  }
  // MIDI Monitor
  if (currentAppType == APP_MONITOR) {
    for (int i = 0; i < 4; i++) {
      int y = i * 8;
      
      if (i < monitor.getActiveNoteCount()) {
        const NoteEvent& note = monitor.getActiveNotes()[i];
        
        if (!note.active) {
          display.setTextColor(SSD1306_WHITE);
        } else {
          display.setTextColor(SSD1306_WHITE);
        }
        
        display.setCursor(0, y);
        display.print("CH");
        display.print(note.channel + 1);
        display.print(" ");
        
        display.setCursor(24, y);
        if (monitor.showNoteNames()) {
          display.print(getNoteName(note.note));
        } else {
          if (note.note < 100) display.print(" ");
          display.print(note.note);
        }
        
        display.setCursor(46, y);
        display.print("V");
        if (note.velocity < 100) display.print(" ");
        if (note.velocity < 10) display.print(" ");
        display.print(note.velocity);
        
        // Затемнение отпущенных нот — рисуем чёрные точки поверх
        if (!note.active) {
          for (int px = 0; px < 64; px += 2) {
            for (int py = y; py < y + 7; py += 2) {
              display.drawPixel(px, py, SSD1306_BLACK);
            }
          }
        }
      }
    }
    
    for (int i = 0; i < 4; i++) {
      int y = i * 8;
      
      if (i < monitor.getActiveCCCount()) {
        const CCEvent& cc = monitor.getActiveCCs()[i];
        
        display.setTextColor(SSD1306_WHITE);
        
        // Формат "CCxx:xxx" = 9 символов = 54px, от правого края
        display.setCursor(74, y);
        display.print("CC");
        if (cc.number < 10) display.print(" ");
        display.print(cc.number);
        display.print(":");
        if (cc.value < 100) display.print(" ");
        if (cc.value < 10) display.print(" ");
        display.print(cc.value);
        
        // Затемнение стабильных CC, ярко при изменении
        if (!cc.dirty) {
          for (int px = 74; px < 128; px += 2) {
            for (int py = y; py < y + 7; py += 2) {
              display.drawPixel(px, py, SSD1306_BLACK);
            }
          }
        }
      }
    }
  }

  display.display();
  MIDI.read();
}

void ui_handleEncoder(int delta) {
  ui_markDirty(UI_DIRTY_FULL);
  if (uiState == UI_MENU) {
    menuPosition = (menuPosition + delta + APP_COUNT) % APP_COUNT;
    return;
  }

  if (uiState == UI_STEP_EDIT && currentAppType == APP_MEL_SEQ) {
    int8_t dir = (delta > 0) ? 1 : -1;
    
    if (melSeq.transposeEditActive) {
      melSeq.adjustTranspose(melSeq.getEditStep(), dir);
      return;
    }
    
    int16_t newStep = (int16_t)melSeq.getEditStep() + dir;
    if (newStep < 0) newStep = melSeq.params.length - 1;
    if (newStep >= melSeq.params.length) newStep = 0;
    melSeq.setEditStepDirect((uint8_t)newStep);
    lastEditStep = newStep;
    lastEditStepValid = true;
    uint8_t newPage = (newStep / STEPS_PER_PAGE) + 1;
    if (newPage != melSeq.params.page && newPage >= 1 && newPage <= 4) {
      melSeq.params.page = newPage;
    }
    return;
  }

  if (stepEditMode && currentAppType == APP_SONG && (uiState == UI_STEP_EDIT || uiState == UI_NAVIGATE || uiState == UI_EDIT)) {
    int8_t dir = (delta > 0) ? 1 : -1;
    
    if (songSeq.stepSelectMode) {
      // Правый столбец: выбор шага
      int16_t newStep = (int16_t)songSeq.getEditStep() + dir;
      if (newStep < 0) newStep = songSeq.params.length - 1;
      if (newStep >= songSeq.params.length) newStep = 0;
      songSeq.setEditStepDirect((uint8_t)newStep);
      uint8_t newPage = (newStep / STEPS_PER_PAGE) + 1;
      if (newPage != songSeq.params.page && newPage >= 1 && newPage <= 4) {
        songSeq.params.page = newPage;
      }
    } else {
      // Левый столбец: параметры шага
      if (uiState == UI_NAVIGATE) {
        uint8_t currentParam = songSeq.getStepEditParam();
        uint8_t step = songSeq.getEditStep();
        bool isPause = (songSeq.getStepParams(step).patternSlot == 0);
        
        // Единый порядок параметров для пауз и паттернов
        // Паузы:     PTRN(0) → PAUSE_LEN(3) → DIV(2)
        // Паттерны:  PTRN(0) → TRANSPOSE(1) → DIV(2)
        // При смене типа шага — сбрасываем параметр на ближайший валидный
        static const uint8_t pauseParams[] = {STEP_PARAM_PATTERN, STEP_PARAM_PAUSE_LEN, STEP_PARAM_DIVIDER};
        static const uint8_t patternParams[] = {STEP_PARAM_PATTERN, STEP_PARAM_TRANSPOSE, STEP_PARAM_DIVIDER};
        const uint8_t* allowedParams = isPause ? pauseParams : patternParams;
        const uint8_t allowedCount = 3;
        
        // Проверяем, валиден ли currentParam для текущего типа шага
        bool valid = false;
        int8_t currentIdx = 0;
        for (uint8_t i = 0; i < allowedCount; i++) {
          if (allowedParams[i] == currentParam) {
            valid = true;
            currentIdx = i;
            break;
          }
        }
        
        // Если невалиден (сменили тип шага) — сбрасываем на первый параметр
        if (!valid) {
          currentIdx = 0;
          currentParam = allowedParams[0];
        }
        
        // Двигаемся по массиву
        currentIdx = currentIdx + dir;
        if (currentIdx >= allowedCount) currentIdx = allowedCount - 1;
        if (currentIdx < 0) currentIdx = 0;
        currentParam = allowedParams[currentIdx];
        
        songSeq.setStepEditParam(currentParam);
      } else if (uiState == UI_EDIT) {
        // Редактирование значения параметра
        uint8_t step = songSeq.getEditStep();
        SongStepParams& sp = songSeq.getStepParams(step);
        uint8_t activeParam = songSeq.getStepEditParam();
        
        if (sp.patternSlot == 0 && activeParam == STEP_PARAM_TRANSPOSE) {
          activeParam = STEP_PARAM_PAUSE_LEN;
        }
        
        switch (activeParam) {
          case STEP_PARAM_PATTERN:
            {
              int16_t newSlot = (int16_t)sp.patternSlot + dir;
              newSlot = constrain(newSlot, 0, 64);
              songSeq.setPatternSlot(step, (uint8_t)newSlot);
            }
            break;
          case STEP_PARAM_TRANSPOSE:
            songSeq.adjustTranspose(step, dir);
            break;
          case STEP_PARAM_PAUSE_LEN:
            {
              int16_t newLen = (int16_t)sp.pauseLength + dir;
              newLen = constrain(newLen, 1, 64);
              songSeq.setPauseLength(step, (uint8_t)newLen);
            }
            break;
          case STEP_PARAM_DIVIDER:
            {
              int16_t newDiv = (int16_t)sp.divider + dir;
              newDiv = constrain(newDiv, 0, 5);
              songSeq.setDivider(step, (uint8_t)newDiv);
            }
            break;
        }
      }
    }
    return;
  }

  if (uiState == UI_NAVIGATE) {
    int scrollOffset = (currentCol == COL_LEFT) ? leftScrollOffset : rightScrollOffset;
    int paramCount = (currentCol == COL_LEFT) ? leftParamCount : rightParamCount;
    int realIndex = scrollOffset + cursorVisualRow;

    realIndex = constrain(realIndex + delta, 0, paramCount - 1);

    if (realIndex < scrollOffset) {
      scrollOffset = realIndex;
      cursorVisualRow = 0;
    } else if (realIndex >= scrollOffset + VISIBLE_ROWS) {
      scrollOffset = realIndex - VISIBLE_ROWS + 1;
      cursorVisualRow = VISIBLE_ROWS - 1;
    } else {
      cursorVisualRow = realIndex - scrollOffset;
    }

    if (currentCol == COL_LEFT) leftScrollOffset = scrollOffset;
    else rightScrollOffset = scrollOffset;

  } else if (uiState == UI_EDIT) {
    ParamDef* col = (currentCol == COL_LEFT) ? leftParams : rightParams;
    int realIndex = (currentCol == COL_LEFT) ? leftScrollOffset : rightScrollOffset;
    realIndex += cursorVisualRow;

    ParamDef& param = col[realIndex];

    // Особая обработка SAVE — не меняем значение
    if (strcmp(param.label, "SAVE") == 0) {
      return;
    }
    
    // HELP — фиктивный параметр, не редактируется
    if (strcmp(param.label, "HELP") == 0) {
      return;
    }
    
    // BRIGHTNESS — 8 уровней (1-8)
    if (strcmp(param.label, "BRIGHT") == 0) {
      *param.value = constrain(*param.value + delta, 1, 8);
      settingsApp.applyBrightness();
      scheduleGlobalSave();
      return;
    }
    
    // CLKIN/CLKOUT/START — применяем сразу
    if (strcmp(param.label, "CLKIN") == 0) {
      *param.value = constrain(*param.value + delta, param.min, param.max);
      clock_setSourceExternal(*param.value == 1);
      scheduleGlobalSave();
      return;
    }
    
    if (strcmp(param.label, "CLKOUT") == 0) {
      *param.value = constrain(*param.value + delta, param.min, param.max);
      clock_setOutEnabled(*param.value == 1);
      scheduleGlobalSave();
      return;
    }

    if (strcmp(param.label, "START") == 0) {
      *param.value = constrain(*param.value + delta, param.min, param.max);
      clock_setTransportEnabled(*param.value == 1);
      scheduleGlobalSave();
      return;
    }

    // BPM глобальный — не крутим при внешнем clock
    if (strcmp(param.label, "BPM") == 0 && clock_isSourceExternal()) {
      return;
    }
    
    int oldValue = *param.value;
    *param.value = constrain(*param.value + delta, param.min, param.max);

    if (strcmp(param.label, "BPM") == 0) {
      clock_setBpm(*param.value);
    }
    
    // При смене PTRN — загружаем паттерн
    if (strcmp(param.label, "PTRN") == 0 && *param.value != oldValue) {
      melSeq.loadFromFile(*param.value - 1);
      scheduleGlobalSave();
    }
    
    // При смене SONG — загружаем песню
    if (strcmp(param.label, "SONG") == 0 && *param.value != oldValue) {
      songSeq.loadFromFile(*param.value - 1);
      songSlot = *param.value;
      scheduleGlobalSave();
    }
    
    // LENGTH — параметр паттерна/песни, не сохраняем в EEPROM
    if (strcmp(param.label, "LENGTH") == 0) {
      if (currentAppType == APP_MEL_SEQ) {
        melSeq.markDirty();
      } else if (currentAppType == APP_SONG) {
        songSeq.markDirty();
      }
    } else if (currentAppType != APP_SETTINGS) {
      scheduleGlobalSave();
    }

    if (currentAppType == APP_ARPEGGIATOR) {
      if (&param == &arpRight[1]) arp.rebuildNotes();
      if (&param == &arpLeft[4]) {
        if (arp.params.hold == 1) {
          arp.enableHold();
          arp.rebuildNotes();
        } else arp.clearHold();
      }
    }
  }
}

void ui_handleEncoderPress(bool shortPress) {
  ui_markDirty(UI_DIRTY_FULL);
  if (uiState == UI_MENU) {
    if (shortPress) {
      ui_setApp((AppType)menuPosition);
      uiState = UI_NAVIGATE;
    }
    return;
  }
  if (uiState == UI_HELP) {
    if (shortPress) {
      uiState = UI_NAVIGATE;
    }
    return;
  }
  if (uiState == UI_STEP_EDIT && currentAppType == APP_MEL_SEQ) {
    if (shortPress) {
      melSeq.transposeEditActive = !melSeq.transposeEditActive;
    }
    return;
  }
  
  if (stepEditMode && currentAppType == APP_SONG && (uiState == UI_STEP_EDIT || uiState == UI_NAVIGATE || uiState == UI_EDIT)) {
    if (shortPress) {
      if (songSeq.stepSelectMode) {
        // В режиме выбора шага (правый столбец) — MUTE шага
        songSeq.toggleMute(songSeq.getEditStep());
      } else {
        // В режиме параметров (левый столбец) — войти/выйти из EDIT
        uiState = (uiState == UI_NAVIGATE) ? UI_EDIT : UI_NAVIGATE;
      }
    }
    return;
  }
  
  if (uiState == UI_STEP_EDIT) {
    return;
  }

  if (shortPress) {
    // В режиме монитора - переключение отображения нот
    if (currentAppType == APP_MONITOR) {
      monitor.toggleNoteDisplay();
      return;
    }
    // Проверяем, не на HELP или SAVE ли мы
    if (uiState == UI_NAVIGATE) {
      ParamDef* col = (currentCol == COL_LEFT) ? leftParams : rightParams;
      int realIndex = (currentCol == COL_LEFT) ? leftScrollOffset : rightScrollOffset;
      realIndex += cursorVisualRow;
      if (realIndex < ((currentCol == COL_LEFT) ? leftParamCount : rightParamCount)) {
        if (strcmp(col[realIndex].label, "HELP") == 0) {
          uiState = UI_HELP;
          return;
        }
        if (strcmp(col[realIndex].label, "SAVE") == 0) {
          if (currentAppType == APP_MEL_SEQ) {
            melSeq.saveToFile(patternSlot - 1);
          } else if (currentAppType == APP_SONG) {
            songSeq.saveToFile(songSlot - 1);
          }
          return;
        }
      }
    }
    uiState = (uiState == UI_NAVIGATE) ? UI_EDIT : UI_NAVIGATE;
  } else {
    uiState = UI_MENU;
    menuPosition = (int)currentAppType;
  }
}

void ui_handleButton(uint8_t pin, bool longPress) {
  ui_markDirty(UI_DIRTY_FULL);
  switch (pin) {
    case BTN_L_R:
      // TAP + L_R = STEP EDIT (для MEL_SEQ)
      if (digitalRead(BTN_TAP) == LOW && currentAppType == APP_MEL_SEQ) {
        if (!longPress) {
          stepEditMode = !stepEditMode;
          uiState = stepEditMode ? UI_STEP_EDIT : UI_NAVIGATE;
          melSeq.stepEditActive = stepEditMode;
          if (stepEditMode) {
            if (lastEditStepValid) {
              melSeq.setEditStepDirect(lastEditStep);
            } else {
              melSeq.setEditStepDirect(melSeq.getCurrentStep());
            }
          }
        }
        tapUsedInCombo = true;
        return;
      }

      // TAP + L_R = STEP EDIT (для SONG)
      if (digitalRead(BTN_TAP) == LOW && currentAppType == APP_SONG && !tapUsedInCombo) {
        if (!longPress) {
          stepEditMode = !stepEditMode;
          if (stepEditMode) {
            uiState = UI_STEP_EDIT;  // Входим в правый столбец — выбор шага (SLOT)
            songSeq.stepEditActive = true;
            songSeq.stepSelectMode = true;
            songSeq.setEditStepDirect(songSeq.getCurrentStep());
          } else {
            uiState = UI_NAVIGATE;
            songSeq.stepEditActive = false;
            songSeq.stepSelectMode = false;
          }
        }
        tapUsedInCombo = true;
        return;
      }

      // В STEP EDIT MEL_SEQ: L_R переключает Tie
      if (stepEditMode && currentAppType == APP_MEL_SEQ && uiState == UI_STEP_EDIT) {
        if (!longPress) {
          melSeq.toggleTie(melSeq.getEditStep());
          scheduleGlobalSave();
        }
        return;
      }
      
      // В STEP EDIT SONG: L_R переключает столбцы (левые параметры / правый выбор шага)
      if (stepEditMode && currentAppType == APP_SONG) {
        if (!longPress) {
          // Запоминаем, были ли мы в режиме EDIT в левом столбце
          static bool songLeftWasEditing = false;
          
          if (songSeq.stepSelectMode) {
            // Переходим ИЗ правого (выбор шага) В левый (параметры)
            songSeq.stepSelectMode = false;
            uiState = songLeftWasEditing ? UI_EDIT : UI_NAVIGATE;
          } else {
            // Переходим ИЗ левого (параметры) В правый (выбор шага)
            songLeftWasEditing = (uiState == UI_EDIT);
            songSeq.stepSelectMode = true;
            uiState = UI_STEP_EDIT;
          }
        }
        return;
      }

      // Если мы в STEP EDIT (любом) — не обрабатываем L_R как переключение колонок
      if (uiState == UI_STEP_EDIT) {
        return;
      }

      // Обычное переключение колонки
      if (currentCol == COL_LEFT) {
        currentCol = COL_RIGHT;
        if (rightScrollOffset + cursorVisualRow >= rightParamCount) {
          cursorVisualRow = rightParamCount - rightScrollOffset - 1;
          if (cursorVisualRow < 0) cursorVisualRow = 0;
        }
      } else {
        currentCol = COL_LEFT;
        if (leftScrollOffset + cursorVisualRow >= leftParamCount) {
          cursorVisualRow = leftParamCount - leftScrollOffset - 1;
          if (cursorVisualRow < 0) cursorVisualRow = 0;
        }
      }
      break;

    case BTN_TAP:
      if (tapUsedInCombo) {
        return;
      }
      
      if (digitalRead(BTN_L_R) == LOW) {
        tapUsedInCombo = true;
        return;
      }

      if (currentAppType == APP_MEL_SEQ) {
        melSeq.tap();
      } else if (currentAppType == APP_SONG) {
        songSeq.tap();
      } else {
        arp.tap();
      }
      break;

    case BTN_PLAY:
      if (digitalRead(BTN_TAP) == LOW && currentAppType == APP_MEL_SEQ) {
        if (!longPress) {
          melSeq.toggleRecord();
        }
        tapUsedInCombo = true;
        return;
      }

      if (uiState == UI_STEP_EDIT && longPress && currentAppType == APP_MEL_SEQ) {
        melSeq.clearStep(melSeq.getEditStep());
      } else if (uiState == UI_STEP_EDIT && longPress && currentAppType == APP_SONG) {
        songSeq.clearStep(songSeq.getEditStep());
      } else if (longPress && currentAppType == APP_MEL_SEQ && !stepEditMode) {
        bool wasEnabled = melSeq.enabled;
        melSeq.clear();
        melSeq.enabled = wasEnabled;
      } else if (longPress && currentAppType == APP_SONG && !stepEditMode) {
        bool wasEnabled = songSeq.enabled;
        songSeq.clear();
        songSeq.enabled = wasEnabled;
      } else if (!longPress) {
          bool before = currentApp->isEnabled();
          currentApp->toggle();
          bool after = currentApp->isEnabled();
          if (before != after) {
            clock_onLocalPlay(after);
          }
          scheduleGlobalSave();
      }
      break;
  }
}