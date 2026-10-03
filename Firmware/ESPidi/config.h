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

// Адреса EEPROM. Блоки идут подряд и не перекрываются; макросы с sizeof() раскрываются там,
// где подключены arp.h / seq_mel.h / seq_song.h / settings.h (midi_handler.cpp).
// При изменении раскладки увеличьте EEPROM_LAYOUT_VERSION: старые данные будут проигнорированы.
#define EEPROM_LAYOUT_VERSION 0xE2
#define EEPROM_VERSION      0   // байт версии раскладки
#define EEPROM_APP_TYPE     1   // текущее приложение
#define EEPROM_SEQ_DATA     2   // номер текущего паттерна
#define EEPROM_SONG_DATA    3   // номер текущей песни
#define EEPROM_ARP_PARAMS   4
#define EEPROM_SEQ_PARAMS   (EEPROM_ARP_PARAMS + sizeof(ArpParams))
#define EEPROM_SONG_PARAMS  (EEPROM_SEQ_PARAMS + sizeof(MelSeqParams))
#define EEPROM_SETTINGS     (EEPROM_SONG_PARAMS + sizeof(SongParams))

#define VISIBLE_ROWS 3
#define MAX_PATTERNS 64
#define PATTERN_DIR "/patterns"

#endif