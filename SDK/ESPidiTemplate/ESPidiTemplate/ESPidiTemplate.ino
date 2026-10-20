// ESPidi HUB firmware template.
// Copy this folder, rename the folder and the .ino, then change ESPIDI_FIRMWARE() below.
// Rules: docs/FIRMWARE_GUIDE.md in the ESPidi-HUB repository.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <ESPidiHub.h>
#include "cover.h"   // ESPIDI_COVER(...): the 64x32 picture the HUB shows next to the name

ESPIDI_FIRMWARE("template", "TEMPLATE", "0.1.0", "Your Name");

namespace pin = espidi::pin;

Adafruit_SSD1306 display(espidi::OLED_WIDTH, espidi::OLED_HEIGHT, &Wire, -1);
HardwareSerial midi(1);
Preferences prefs;

uint32_t bootCount = 0;
bool filesOk = false;
uint32_t encPressedAt = 0;

void sendNote(uint8_t note, uint8_t vel) {
  uint8_t msg[3] = {(uint8_t)(vel ? 0x90 : 0x80), note, vel};
  midi.write(msg, 3);
}

void setup() {
  for (uint8_t p : {pin::PLAY, pin::TAP, pin::PAGE, pin::ENC_SW, pin::ENC_A, pin::ENC_B}) pinMode(p, INPUT_PULLUP);

  Wire.begin(pin::OLED_SDA, pin::OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, espidi::OLED_ADDR);
  midi.begin(espidi::MIDI_BAUD, SERIAL_8N1, pin::MIDI_RX, pin::MIDI_TX);

  // Settings: your own NVS namespace (= descriptor id). Never call nvs_flash_erase().
  prefs.begin("template");
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);

  // Files: the shared LittleFS, your own folder.
  filesOk = espidi::mountFiles();
  if (filesOk) {
    File f = LittleFS.open(espidi::dataDir("template") + "/hello.txt", "w");
    if (f) {
      f.printf("boot %lu\n", (unsigned long)bootCount);
      f.close();
    }
  }
}

void loop() {
  static bool playWas = false;
  bool play = digitalRead(pin::PLAY) == LOW;
  if (play != playWas) {
    sendNote(60, play ? 100 : 0);
    playWas = play;
  }

  // Hold the encoder button for 2 s to go back to the HUB.
  if (digitalRead(pin::ENC_SW) == LOW) {
    if (!encPressedAt) encPressedAt = millis();
    if (millis() - encPressedAt > 2000) espidi::rebootToHub();
  } else {
    encPressedAt = 0;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("TEMPLATE  slot %d", espidi::slot());
  display.setCursor(0, 8);
  display.printf("boots %lu  files %s", (unsigned long)bootCount, filesOk ? "ok" : "--");
  display.setCursor(0, 16);
  display.printf("PLAY%c TAP%c PAGE%c ENC%c", play ? '*' : '.', digitalRead(pin::TAP) ? '.' : '*',
                 digitalRead(pin::PAGE) ? '.' : '*', digitalRead(pin::ENC_SW) ? '.' : '*');
  display.setCursor(0, 24);
  display.print("hold ENC 2s: HUB");
  display.display();
  delay(10);
}
