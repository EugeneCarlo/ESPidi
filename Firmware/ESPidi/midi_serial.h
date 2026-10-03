#ifndef MIDI_SERIAL_H
#define MIDI_SERIAL_H

#include <Arduino.h>

// Прослойка между UART и библиотекой MIDI.
// По спецификации MIDI любой статус-байт, кроме реалтайма, завершает незакрытый SysEx.
// Библиотека этого не делает: если F7 потерян (обрезанный SysEx), она складывает все
// следующие ноты и CC в буфер SysEx и «глохнет» до перезагрузки. Прослойка в таком случае
// сначала отдаёт библиотеке F7, а затем сам статус-байт.
class MidiSerial {
public:
    explicit MidiSerial(HardwareSerial& port) : port_(port) {}

    void begin(unsigned long baud) { port_.begin(baud); }

    int available() { return pending_ >= 0 ? 1 : port_.available(); }

    int read() {
        int c;
        if (pending_ >= 0) {
            c = pending_;
            pending_ = -1;
        } else {
            c = port_.read();
            if (c < 0) return c;
            if (inSysEx_ && c >= 0x80 && c < 0xF8 && c != 0xF7) {
                pending_ = c;  // статус-байт отдадим следующим вызовом
                c = 0xF7;      // а сейчас — завершение SysEx
            }
        }
        if (c == 0xF0) inSysEx_ = true;
        else if (c >= 0x80 && c < 0xF8) inSysEx_ = false;  // F7 или любой другой не-реалтайм статус
        return c;
    }

    size_t write(uint8_t b) { return port_.write(b); }

private:
    HardwareSerial& port_;
    int pending_ = -1;
    bool inSysEx_ = false;
};

#endif
