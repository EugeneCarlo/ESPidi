#ifndef CONFIG_H
#define CONFIG_H

#define SDA_PIN     8
#define SCL_PIN     9
#define ENC_A       6
#define ENC_B       5
#define ENC_BTN     4
#define BTN_L_R     3
#define BTN_TAP     2
#define BTN_PLAY    1
#define MIDI_RX     7
#define MIDI_TX     10

#define OLED_WIDTH  128
#define OLED_HEIGHT 32
#define OLED_ADDR   0x3C

#define MAX_HELD_NOTES 8

#define DEBOUNCE_MS     30
#define LONG_PRESS_MS   600
#define TAP_TIMEOUT_MS  2000
#define SAVE_DELAY_MS 5000

// Адреса EEPROM
#define EEPROM_START        0
#define EEPROM_APP_TYPE     EEPROM_START
#define EEPROM_ARP_PARAMS   (EEPROM_APP_TYPE + sizeof(uint8_t))
#define EEPROM_SEQ_PARAMS   (EEPROM_ARP_PARAMS + sizeof(ArpParams))
#define EEPROM_SEQ_DATA     (EEPROM_SEQ_PARAMS + sizeof(MelSeqParams))
#define EEPROM_SONG_PARAMS  (EEPROM_SEQ_DATA + 1)
#define EEPROM_SONG_DATA    (EEPROM_SONG_PARAMS + sizeof(SongParams))
#define EEPROM_SETTINGS     (EEPROM_SONG_DATA + 1)// EEPROM_SEQ_DATA хранит номер текущего паттерна (1 байт)
// EEPROM_SONG_PARAMS хранит SongParams
// EEPROM_SONG_DATA хранит номер текущей песни (1 байт)

#define VISIBLE_ROWS 3
#define MAX_PATTERNS 64
#define PATTERN_DIR "/patterns"

#endif