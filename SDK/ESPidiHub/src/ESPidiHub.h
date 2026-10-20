// ESPidi HUB SDK — what a firmware needs to run from an ESPidi HUB slot.
//
//   #include <ESPidiHub.h>
//   ESPIDI_FIRMWARE("redsea", "RED SEA", "1.1.0", "Daniil Vlasenko");
//
// Header-only. Rules: docs/FIRMWARE_GUIDE.md (people), AGENTS.md (AI agents).
// Covers (the picture the HUB shows at start): sdk/ESPidiHub/COVERS.md.
#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace espidi {

constexpr const char *SDK_VERSION = "0.1.0";

// ---- Hardware (ESPidi HARDWARE.MD). Buttons and encoder: INPUT_PULLUP, active LOW.
namespace pin {
constexpr uint8_t PLAY = 1;
constexpr uint8_t TAP = 2;       // strapping pin: the chip may not boot if it is LOW at reset
constexpr uint8_t PAGE = 3;      // labelled "L/R" on the board
constexpr uint8_t ENC_SW = 4;
constexpr uint8_t ENC_B = 5;
constexpr uint8_t ENC_A = 6;
constexpr uint8_t MIDI_RX = 7;
constexpr uint8_t OLED_SDA = 8;  // strapping pin
constexpr uint8_t OLED_SCL = 9;  // strapping pin (BOOT)
constexpr uint8_t MIDI_TX = 10;
}  // namespace pin

constexpr uint8_t OLED_ADDR = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 32;
constexpr uint32_t MIDI_BAUD = 31250;

// Name of the shared LittleFS partition (also the default of LittleFS.begin()).
constexpr const char *FILES_PARTITION = "spiffs";

// ---- Firmware descriptor. The installer and the HUB look for it to show the name.
// Layout must match tools/espidi_hub.py (DESC) and hub/main/slots.c (fw_desc_t).
struct __attribute__((aligned(4))) FirmwareDescriptor {
  char magic[12];    // "ESPIDI-FW-1"
  char id[16];       // short lowercase id: NVS namespace and folder name, max 15 chars
  char name[24];     // shown by the HUB, max 23 chars
  char version[16];  // max 15 chars
  char author[24];   // max 23 chars
};
static_assert(sizeof(FirmwareDescriptor) == 92, "descriptor layout");

// ---- Cover: 64x32, 1 bit per pixel, rows top to bottom, 8 bytes per row, MSB = leftmost
// pixel (the Adafruit GFX drawBitmap() / image2cpp "horizontal" format), 1 = lit.
// The HUB shows it next to the name when it starts the firmware.
// Layout must match tools/espidi_hub.py (FW_COVER_MAGIC) and hub/main/slots.c.
constexpr uint8_t COVER_WIDTH = 64;
constexpr uint8_t COVER_HEIGHT = 32;
struct __attribute__((aligned(4))) Cover {
  char magic[12];      // "ESPIDI-CV-1"
  uint8_t bits[256];
};
static_assert(sizeof(Cover) == 268, "cover layout");

// ---- Where the firmware runs.
inline const esp_partition_t *hubPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "hub");
}

// True when the firmware was started by the ESPidi HUB.
inline bool underHub() {
  return hubPartition() != nullptr;
}

// 1 or 2 — the slot this firmware runs from; 0 when there is no HUB.
inline int slot() {
  const esp_partition_t *p = esp_ota_get_running_partition();
  if (!p || !underHub()) return 0;
  if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) return 1;
  if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) return 2;
  return 0;
}

// Restarts into the HUB (splash and slot choice). Without the HUB it is a plain restart.
// A plain esp_restart() / ESP.restart() restarts this same firmware.
[[noreturn]] inline void rebootToHub() {
  if (const esp_partition_t *hub = hubPartition()) esp_ota_set_boot_partition(hub);
  esp_restart();
  for (;;) {}
}

// ---- Shared file storage.
// Mounts the shared LittleFS partition. Never destroys data: a partition that does not
// mount is formatted only if it is completely blank (fresh install), otherwise this
// returns false and leaves it alone. Keep your files in "/<your id>/".
inline bool mountFiles(const char *basePath = "/littlefs") {
  if (LittleFS.begin(false, basePath, 10, FILES_PARTITION)) return true;
  const esp_partition_t *p =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, FILES_PARTITION);
  if (!p) return false;
  uint8_t buf[256];
  for (uint32_t off = 0; off < 8192; off += sizeof(buf)) {
    if (esp_partition_read(p, off, buf, sizeof(buf)) != ESP_OK) return false;
    for (uint8_t b : buf)
      if (b != 0xFF) return false;
  }
  return LittleFS.begin(true, basePath, 10, FILES_PARTITION);
}

// Creates "/<id>" on the mounted file storage and returns it, e.g. dataDir("redsea").
inline String dataDir(const char *id) {
  String dir = String("/") + id;
  if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
  return dir;
}

}  // namespace espidi

// Put once in the sketch, at file scope:
//   ESPIDI_FIRMWARE("redsea", "RED SEA", "1.1.0", "Daniil Vlasenko");
// Too long a string is a compile error. `retain` keeps it in the binary even though no
// code refers to it.
#define ESPIDI_FIRMWARE(id, name, version, author)                                  \
  extern "C" __attribute__((used, retain)) const espidi::FirmwareDescriptor         \
      espidi_firmware_descriptor = {"ESPIDI-FW-1", id, name, version, author}

// Optional, once per sketch: the 256 bytes of a 64x32 cover — see COVERS.md. Don't type them:
// in an ESPidi-ready repository cover.h is generated from the cover named in espidi.json
//   python3 tools/espidi_hub.py repo . --sync-cover
// (or: python3 tools/espidi_hub.py cover my-cover.png > cover.h). More than 256 bytes is a compile error.
#define ESPIDI_COVER(...)                                                            \
  extern "C" __attribute__((used, retain)) const espidi::Cover                      \
      espidi_firmware_cover = {"ESPIDI-CV-1", {__VA_ARGS__}}
