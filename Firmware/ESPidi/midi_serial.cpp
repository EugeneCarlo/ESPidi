#include "midi_serial.h"
#include <hal/uart_ll.h>

// Сколько байт держим в аппаратном FIFO UART1 (MIDI OUT). Байт на 31250 бод — 0,32 мс:
// 3 байта ≈ 1 мс, этого хватает, чтобы выход не простаивал между вызовами pump() (раз в 0,5 мс),
// и Clock стоит в очереди за ними не дольше ~1 мс.
static const uint32_t FIFO_KEEP = 3;

static portMUX_TYPE s_txMux = portMUX_INITIALIZER_UNLOCKED;

size_t MidiSerial::write(uint8_t b) {
    if (b >= 0xF8) {
        // Реалтайм: вне общей очереди. Переполнение здесь возможно только если pump() не
        // вызывается вовсе (64 такта подряд) — тогда старый Clock уже бесполезен, теряем новый.
        portENTER_CRITICAL(&s_txMux);
        uint16_t next = (rtHead_ + 1) & (RT_SIZE - 1);
        if (next != rtTail_) {
            rt_[rtHead_] = b;
            rtHead_ = next;
        }
        portEXIT_CRITICAL(&s_txMux);
        pump();  // обычно уходит сразу
        return 1;
    }
    for (;;) {
        portENTER_CRITICAL(&s_txMux);
        uint16_t next = (txHead_ + 1) & (TX_SIZE - 1);
        bool ok = next != txTail_;
        if (ok) {
            tx_[txHead_] = b;
            txHead_ = next;
        }
        portEXIT_CRITICAL(&s_txMux);
        if (ok) return 1;
        // Очередь полна (больше ~300 сообщений за раз): ждём, пока UART освободит место.
        // Задача движка в это время заблокирована нами, поэтому докладываем FIFO сами.
        pump();
        delayMicroseconds(100);
    }
}

void MidiSerial::pump() {
    portENTER_CRITICAL(&s_txMux);
    uart_dev_t* hw = UART_LL_GET_HW(1);
    uint32_t inFifo = UART_LL_FIFO_DEF_LEN - uart_ll_get_txfifo_len(hw);
    while (inFifo < FIFO_KEEP) {
        uint8_t b;
        if (rtTail_ != rtHead_) {
            b = rt_[rtTail_];
            rtTail_ = (rtTail_ + 1) & (RT_SIZE - 1);
        } else if (txTail_ != txHead_) {
            b = tx_[txTail_];
            txTail_ = (txTail_ + 1) & (TX_SIZE - 1);
        } else {
            break;
        }
        uart_ll_write_txfifo(hw, &b, 1);
        inFifo++;
    }
    portEXIT_CRITICAL(&s_txMux);
}

uint16_t MidiSerial::queued() {
    portENTER_CRITICAL(&s_txMux);
    uint16_t n = ((rtHead_ - rtTail_) & (RT_SIZE - 1)) + ((txHead_ - txTail_) & (TX_SIZE - 1));
    portEXIT_CRITICAL(&s_txMux);
    return n;
}
