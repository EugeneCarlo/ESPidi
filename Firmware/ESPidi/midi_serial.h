#ifndef MIDI_SERIAL_H
#define MIDI_SERIAL_H

#include <Arduino.h>

// Прослойка между UART и библиотекой MIDI.
//
// Приём. По спецификации MIDI любой статус-байт, кроме реалтайма, завершает незакрытый SysEx.
// Библиотека этого не делает: если F7 потерян (обрезанный SysEx), она складывает все
// следующие ноты и CC в буфер SysEx и «глохнет» до перезагрузки. Прослойка в таком случае
// сначала отдаёт библиотеке F7, а затем сам статус-байт.
//
// Отправка. Байты не уходят в UART сразу, а встают в очередь; pump() докладывает их в
// аппаратный FIFO понемногу, держа в нём не больше нескольких байт. Реалтайм (Clock, Start,
// Stop…) идёт отдельной очередью вне общей и уходит следующим же байтом: Clock не ждёт, пока
// уйдёт пачка NoteOff (128 нот — это 120 мс на скорости MIDI). Реалтайм-байт по спецификации
// можно вставлять даже внутрь другого сообщения.
//
// write() вызывается под engine_lock() (см. engine.h), pump() — из задачи движка.
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

    size_t write(uint8_t b);
    void pump();          // докладывает очередь в FIFO UART (вызывать часто, раз в ≤ 1 мс)
    uint16_t queued();    // байт в обеих очередях (для замеров)

private:
    static const uint16_t RT_SIZE = 64;     // степени двойки
    static const uint16_t TX_SIZE = 1024;   // ~10 мс тактов при самой длинной пачке нот

    HardwareSerial& port_;
    int pending_ = -1;
    bool inSysEx_ = false;

    uint8_t rt_[RT_SIZE];
    uint8_t tx_[TX_SIZE];
    volatile uint16_t rtHead_ = 0, rtTail_ = 0;
    volatile uint16_t txHead_ = 0, txTail_ = 0;
};

#endif
