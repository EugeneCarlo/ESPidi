# ESPidi HUB SDK

Everything for a firmware that shares an ESPidi with another one under [ESPidi HUB](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB): the HUB keeps two firmwares on the board and starts one at power-on (PLAY → slot 1, PAGE → slot 2, nothing → the last one). Both are installed from the browser at [espidi-hub.github.io](https://espidi-hub.github.io/).

| Folder | |
|---|---|
| [`ESPidiHub/`](ESPidiHub) | Arduino library, one header: the ESPidi pin map, the name and version the HUB shows, a 64×32 cover, safe shared settings and files, the way back to the HUB |
| [`ESPidiTemplate/`](ESPidiTemplate) | a firmware to start from — display, buttons, MIDI out, settings, files, a long press back to the HUB — laid out as a repository with a build workflow |

How to write a firmware: [docs/FIRMWARE.md](../docs/FIRMWARE.md).

**Install:** copy `ESPidiHub/` into your Arduino `libraries` folder, or pass `--library SDK/ESPidiHub` to arduino-cli. Board: ESP32C3 Dev Module, Espressif core 3.x.

**Tools:** `<ESPidi-HUB>` in the docs is a clone of the ESPidi HUB repository. Its `tools/espidi_hub.py` checks a build, installs it over USB (with `esptool`) and turns a picture into `cover.h`.

This is a copy of SDK 0.1.0. The current version is in the ESPidi HUB repository: [sdk/ESPidiHub](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/tree/main/sdk/ESPidiHub), [templates/arduino-firmware](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/tree/main/templates/arduino-firmware). License: MIT, see [LICENSE](LICENSE).

---

# ESPidi HUB SDK — по-русски

Всё для прошивки, которая делит ESPidi со второй прошивкой под [ESPidi HUB](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB): HUB хранит на плате две прошивки и при включении запускает одну из них (PLAY → слот 1, PAGE → слот 2, без кнопки — последняя). Обе ставятся из браузера на [espidi-hub.github.io](https://espidi-hub.github.io/).

| Папка | |
|---|---|
| [`ESPidiHub/`](ESPidiHub) | Arduino-библиотека из одного заголовка: распиновка ESPidi, имя и версия для HUB, обложка 64×32, безопасные общие настройки и файлы, возврат в HUB |
| [`ESPidiTemplate/`](ESPidiTemplate) | прошивка, с которой удобно начать, — экран, кнопки, MIDI-выход, настройки, файлы, долгое нажатие для возврата в HUB — в виде репозитория с workflow сборки |

Как писать прошивку: [docs/FIRMWARE.md](../docs/FIRMWARE.md#как-написать-прошивку-для-espidi-hub).

**Установка:** скопируйте `ESPidiHub/` в папку `libraries` Arduino или передайте `--library SDK/ESPidiHub` в arduino-cli. Плата: ESP32C3 Dev Module, ядро Espressif 3.x.

**Инструменты:** `<ESPidi-HUB>` в документации — клон репозитория ESPidi HUB. Его `tools/espidi_hub.py` проверяет сборку, ставит её по USB (через `esptool`) и превращает картинку в `cover.h`.

Это копия SDK 0.1.0. Актуальная версия — в репозитории ESPidi HUB: [sdk/ESPidiHub](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/tree/main/sdk/ESPidiHub), [templates/arduino-firmware](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/tree/main/templates/arduino-firmware). Лицензия: MIT, см. [LICENSE](LICENSE).
