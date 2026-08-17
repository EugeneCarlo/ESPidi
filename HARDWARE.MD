## GPIO Map

| GPIO | Function | Mode / Interface | Active |
|---:|---|---|---|
| GPIO 0 | Unassigned | — | — |
| GPIO 1 | PLAY Button | Input / Pull-up | LOW |
| GPIO 2 | TAP Button | Input / Pull-up | LOW |
| GPIO 3 | L/R Button | Input / Pull-up | LOW |
| GPIO 4 | Encoder Button | Input / Pull-up | LOW |
| GPIO 5 | Encoder B | Input / Pull-up | — |
| GPIO 6 | Encoder A | Input / Pull-up | — |
| GPIO 7 | MIDI RX | UART RX | — |
| GPIO 8 | OLED SDA | I²C SDA | — |
| GPIO 9 | OLED SCL | I²C SCL | — |
| GPIO 10 | MIDI TX | UART TX | — |

## OLED

| Parameter | Value |
|---|---|
| Controller | SSD1306 |
| Resolution | 128×32 |
| Address | `0x3C` |
| Interface | I²C |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

## Rotary Encoder

| Signal | GPIO |
|---|---:|
| A | GPIO 6 |
| B | GPIO 5 |
| Button | GPIO 4 |

**Encoder inputs:** `INPUT_PULLUP`  
**Encoder button:** Active LOW

## Buttons

| Button | GPIO | Mode | Active |
|---|---:|---|---|
| L/R | GPIO 3 | `INPUT_PULLUP` | LOW |
| TAP | GPIO 2 | `INPUT_PULLUP` | LOW |
| PLAY | GPIO 1 | `INPUT_PULLUP` | LOW |

## MIDI

| Signal | GPIO | Interface |
|---|---:|---|
| MIDI IN | GPIO 7 | UART RX |
| MIDI OUT | GPIO 10 | UART TX |

**UART:** `31250 baud, 8N1`

## Firmware Configuration

```cpp
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
