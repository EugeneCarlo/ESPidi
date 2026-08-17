# ESPidi

**An open-source ESP32-C3 hardware platform for building MIDI controllers and expanding the capabilities of existing instruments.**

ESPidi is a compact, ready-to-use MIDI platform based on the ESP32-C3.

![ESPidi](/Image/photo.jpg)

You can use the included firmware, modify it, or write your own.

## Features & Configuration

* ESP32-C3 Super Mini
* MIDI IN (5-pin DIN or 3.5 mm Jack)
* MIDI OUT (5-pin DIN or 3.5 mm Jack)
* 128×32 OLED (SSD1306 0.91")
* Rotary encoder with push button (EC11)
* 3 additional buttons
* Built-in 10440 rechargeable battery
* DIY development and rapid prototyping

A future hardware revision is planned with a 128×64 OLED and further expansion.

## What Can You Build?

ESPidi is not tied to a specific type of MIDI device.

The platform can be used for:

* MIDI controllers
* Sequencers
* Arpeggiators
* CC controllers
* MIDI LFOs
* MIDI loopers
* MIDI monitors
* MIDI Clock
* Experimental MIDI instruments
* Custom wireless MIDI devices

The hardware is universal: **the firmware determines what the device does.**

The current version provides three additional GPIOs with configuration limitations. **Please refer to the ESP32-C3 datasheet before using them.**

## Example Firmware

The repository includes example firmware with the following modules:

* Arpeggiator
* Melodic Sequencer
* Song Sequencer
* MIDI Monitor
* MIDI Clock
* MIDI Start / Stop / Continue
* OLED UI
* Settings and data storage

This is not a fixed firmware for the device, but a working example, a development base, and a reference implementation.

## Firmware Development

The current firmware is divided into modules for the hardware, controls, and applications.

ESPidi is suitable for both conventional embedded development and AI-assisted programming.

You can start with the Example Firmware and gradually turn the platform into a specialized MIDI controller.

You can:

1. Build the existing firmware;
2. Modify individual modules;
3. Add a new mode;
4. Use the code as a reference;
5. Replace the firmware completely.

See [FIRMWARE.md](docs/FIRMWARE.md) and [BUILDING.md](docs/BUILDING.md).

## Open Hardware

The repository contains the files required to build the device yourself:

* [Schematic](Schematic/Scheme%20ESPidi.jpg)
* [GERBER file - main PCB](/PCB/Gerber_ESPIDI_PCB_ESPIDI-BLANK.zip)
* [GERBER file - panel](/PCB/Gerber_ESPidi_BreadBoard_PCB_ESPidi_BreadBoard.zip)
* [BOM](/BOM/BOM.md)
* [Firmware](/Firmware/ESPidi/)
* [Hardware](/HARDWARE.MD)

## Community

The project is intended for:

* Custom firmware
* Forks
* MIDI applications
* Hardware modifications
* Documentation improvements
* Bug fixes
* New modes and modules

## License

ESPidi is released under the **GNU General Public License v3.0**.

See [LICENSE](LICENSE) for the complete license text.

---

# Русский

## ESPidi

**Открытая аппаратная платформа на ESP32-C3 для создания MIDI-контроллеров и расширения возможностей инструментов.**

ESPidi - компактная готовая MIDI-платформа на базе ESP32-C3.
![ESPidi](Image/photo)
Можно использовать готовую прошивку, изменить её или написать собственную.

## Возможности и конфигурация

* ESP32-C3 Super Mini
* MIDI IN (Din 5 или Jack 3.5mm)
* MIDI OUT (Din 5 или Jack 3.5mm)
* OLED 128×32 (SSD1306 0.91")
* энкодер с кнопкой (EC11)
* 3 дополнительные кнопки
* Встроенный аккумулятор (10440)
* DIY-разработка и быстрое прототипирование

В следующей аппаратной версии планируется OLED 128×64 и дальнейшее расширение.

## Что можно создать?

ESPidi не привязан к конкретному типу MIDI-устройства.

Платформа может использоваться для:

* MIDI-контроллеров
* Секвенсоров
* Арпеджиаторов
* CC-контроллеров
* MIDI LFO
* MIDI луперов
* MIDI монитора
* MIDI Clock
* Экспериментальных MIDI-инструментов
* Собственных беспроводных MIDI-устройств

Аппаратная часть универсальна: назначение устройства определяет прошивка.

Текущая версия имеет три дополнительных GPIO с ограничениями конфигурации, обязательно ознакомьтесь с даташитом ESP32C3.

## Example Firmware

В репозитории находится пример прошивки с модулями:

* Arpeggiator
* Melodic Sequencer
* Song Sequencer
* MIDI Monitor
* MIDI Clock
* MIDI Start / Stop / Continue
* OLED UI
* настройки и хранение данных

Это не фиксированная прошивка устройства, а рабочий пример, база для разработки и справочная реализация.

## Разработка прошивки

Текущая прошивка разделена на модули для аппаратной части, органов управления и приложений.

ESPidi подходит как для обычной embedded-разработки, так и для AI-assisted programming.

Можно начать с Example Firmware и постепенно превратить платформу в специализированный MIDI-контроллер.

Можно:

1. собрать существующую прошивку;
2. изменить отдельные модули;
3. добавить новый режим;
4. использовать код как пример;
5. полностью заменить прошивку.

См. [FIRMWARE.md](docs/FIRMWARE.md) и [BUILDING.md](docs/BUILDING.md).

## Открытое железо

В репозитории опубликованы файлы для самостоятельного создания устройства:

* [Схема](Schematic/Scheme%20ESPidi.jpg);
* [GERBER файл - основной](/PCB/Gerber_ESPIDI_PCB_ESPIDI-BLANK.zip)
* [GERBER файл - панель](/PCB/Gerber_ESPidi_BreadBoard_PCB_ESPidi_BreadBoard.zip)
* [BOM](/BOM/BOM.md)
* [Прошивка](/Firmware/ESPidi/)
* [Подключения](/HARDWARE.MD)

## Сообщество

Проект рассчитан на:

* собственные прошивки;
* форки;
* MIDI-приложения;
* аппаратные модификации;
* улучшение документации;
* исправления ошибок;
* новые режимы и модули.

## Лицензия

ESPidi распространяется под **GNU General Public License v3.0**.

Полный текст находится в [LICENSE](LICENSE).
