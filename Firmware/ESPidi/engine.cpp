#include "engine.h"
#include <esp_timer.h>
#include "hardware.h"
#include "clock_engine.h"

static SemaphoreHandle_t s_lock = nullptr;
static TaskHandle_t s_task = nullptr;
static esp_timer_handle_t s_timer = nullptr;
static esp_timer_handle_t s_tickTimer = nullptr;  // одноразовый: точно на момент следующего такта

static const uint32_t ENGINE_PERIOD_US = 500;  // шаг обслуживания: такт опаздывает не больше чем на 0,5 мс

// До engine_begin() (в setup) блокировка не нужна — работает один поток.
void engine_lock() {
    if (s_lock) xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
}

void engine_unlock() {
    if (s_lock) xSemaphoreGiveRecursive(s_lock);
}

static void onTimer(void*) {
    if (s_task) xTaskNotifyGive(s_task);
}

static void engineTask(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
        engine_lock();
        // Входящий MIDI: библиотека разбирает по байту за вызов
        for (int n = 0; n < 96 && midiSerial.available() > 0; n++) {
            MIDI.read();
        }
        clock_update();      // внутренние такты по расписанию, контроль потери внешнего Clock
        midiSerial.pump();   // отправка накопленного MIDI (Clock — вне очереди)
        engine_unlock();

        // Такт должен уйти раньше следующего периодического пробуждения — будим задачу точно
        // к его моменту, а не по сетке 0,5 мс.
        unsigned long due;
        if (clock_nextTickDue(&due)) {
            long dt = (long)(due - micros());
            if (dt > 0 && dt <= (long)ENGINE_PERIOD_US) {
                esp_timer_stop(s_tickTimer);
                esp_timer_start_once(s_tickTimer, (uint64_t)dt);
            }
        }
    }
}

void engine_begin() {
    s_lock = xSemaphoreCreateRecursiveMutex();  // мьютекс с наследованием приоритета
    xTaskCreate(engineTask, "midi_engine", 6144, nullptr, configMAX_PRIORITIES - 4, &s_task);

    esp_timer_create_args_t args = {};
    args.callback = onTimer;
    args.name = "engine_tick";
    esp_timer_create(&args, &s_timer);
    args.name = "engine_clock";
    esp_timer_create(&args, &s_tickTimer);
    esp_timer_start_periodic(s_timer, ENGINE_PERIOD_US);
}
