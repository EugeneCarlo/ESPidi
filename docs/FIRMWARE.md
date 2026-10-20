# Writing a firmware for ESPidi HUB

[ESPidi HUB](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB) keeps two firmwares on one ESPidi board and starts one of them at power-on; the [composer site](https://espidi-hub.github.io/) writes them to the board from the browser. This guide is everything a firmware needs to live in a slot; the SDK and a template are in [SDK/](../SDK). AI agents: the same rules in checklist form are in [AGENTS.md](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/AGENTS.md). Русская версия: [ниже](#как-написать-прошивку-для-espidi-hub).

## What your firmware gets

- A **slot**: PLAY starts slot 1, PAGE starts slot 2, or the HUB starts the one used last time.
- **Settings**: the shared NVS (16 KB), in your own namespace.
- **Files**: the shared 1 MB LittleFS, in your own folder.
- **Space**: up to 2.62 MB of code when the other slot is empty; otherwise whatever is left next to it. You don't pick a size — the installer does.
- **The whole board** while it runs: display, buttons, encoder, MIDI, Wi-Fi/BLE.

A firmware that follows the rules below does not need to know about the HUB at all. RED SEA, for example, runs unmodified.

## Quick start (Arduino)

1. Install the ESP32 core (Espressif, 3.x) and select **ESP32C3 Dev Module**.
2. Install the SDK: copy [`SDK/ESPidiHub`](../SDK/ESPidiHub) to your Arduino `libraries` folder (or pass `--library SDK/ESPidiHub` to arduino-cli).
3. Start from [`SDK/ESPidiTemplate`](../SDK/ESPidiTemplate).
4. Put a descriptor in the sketch — the HUB shows its name:

```cpp
#include <ESPidiHub.h>
ESPIDI_FIRMWARE("myarp", "MY ARP", "1.0.0", "Your Name");
```

`id` (max 15 characters, lowercase) is also your NVS namespace and your folder name. `name` and `author` max 23 characters, `version` max 15. A longer string is a compile error. Reserved ids — the HUB and the SDK use these NVS namespaces: `espidihub`, `espidi_pad`, `nimble_bond`.

5. Optionally, give it a cover — see below — and make the repository ESPidi-ready (see Publishing): then the composer site installs it from a link.

## Cover

When the HUB starts your firmware it shows a 64×32 picture next to the name and version; the composer site uses it as the firmware's thumbnail. Draw it black and white (white = lit), 64×32 or a multiple (128×64, …), without the name in it, save it as `cover.png` in your repository and name it in `espidi.json`. `cover.h` for the sketch is generated from it.

Everything about covers — drawing, where they come from, checking, the format: **[SDK/ESPidiHub/COVERS.md](../SDK/ESPidiHub/COVERS.md)**.

## Rules

**Must**

1. Target the ESP32-C3 and build an ordinary app image. The file to install is `<sketch>.ino.bin`, **not** `*.merged.bin` and not the bootloader or partition files.
2. Use the ESPidi pin map (`espidi::pin::…`).
3. Keep settings in NVS under **your own namespace** (`Preferences prefs; prefs.begin("<id>");`).
4. Keep files on the shared LittleFS in **`/<id>/`**, mounted with `espidi::mountFiles()`.
5. Fit the space the installer reports (`espidi_hub.py check`).

**Must not**

1. Erase or format shared storage: no `nvs_flash_erase()`, no `LittleFS.format()`, no `LittleFS.begin(true)`; `Preferences::clear()` only on your own namespace. Another firmware's settings and files live there too.
2. Use SPIFFS or FFat on the shared partition — only LittleFS. A different file system would make the partition unreadable for everyone.
3. Use OTA updates (`Update`, `esp_ota_begin()`, `esp_https_ota()`), call `esp_ota_set_boot_partition()` or write otadata — slot selection belongs to the HUB. To leave, call `espidi::rebootToHub()`.
4. Write raw flash or change the partition table.
5. Rely on the board booting with TAP held: GPIO2 is a strapping pin.

## Hardware

The pins of [HARDWARE.MD](../HARDWARE.MD), as the SDK names them:

| Signal | GPIO | Notes |
|---|---|---|
| PLAY | 1 | button, `INPUT_PULLUP`, active LOW |
| TAP | 2 | button; strapping pin — must not be LOW at reset |
| PAGE | 3 | button, labelled L/R on the board |
| Encoder button | 4 | |
| Encoder B / A | 5 / 6 | `INPUT_PULLUP` |
| MIDI RX / TX | 7 / 10 | UART, 31250 baud, 8N1 |
| OLED SDA / SCL | 8 / 9 | SSD1306 128×32, I²C address `0x3C`; both strapping pins |

Constants: `espidi::pin::PLAY` … `espidi::pin::MIDI_TX`, `espidi::OLED_ADDR`, `espidi::OLED_WIDTH`, `espidi::OLED_HEIGHT`, `espidi::MIDI_BAUD`.

## Storage

**Settings** — NVS is small (16 KB for everyone). Store a handful of values, not banks of data:

```cpp
Preferences prefs;
prefs.begin("myarp");            // = descriptor id
uint8_t bpm = prefs.getUChar("bpm", 120);
```

**Files** — presets, patterns, MIDI files:

```cpp
if (espidi::mountFiles()) {
  String dir = espidi::dataDir("myarp");   // creates /myarp
  File f = LittleFS.open(dir + "/preset1.json", "w");
}
```

`mountFiles()` formats the partition only if it is completely blank (fresh install). If it fails to mount for any other reason it returns `false` and leaves the data alone — run without files in that case.

## Size

- The installer rounds your image up to 64 KB and checks it against the free space.
- Alone on the board: up to **2.62 MB**. Next to another firmware: 2.69 MB minus that firmware.
- The Arduino IDE limits sketches to 1.25 MB with the default partition scheme. If yours is bigger, choose **Tools → Partition Scheme → Huge APP**; this only lifts Arduino's check — the HUB layout is not affected.

## Boot behaviour

| Event | What starts |
|---|---|
| Power on, RESET button | the HUB |
| `ESP.restart()` / `esp_restart()` | your firmware again |
| Crash, watchdog | your firmware again |
| Deep-sleep wake | your firmware |
| `espidi::rebootToHub()` | the HUB |

Optional helpers: `espidi::underHub()` — started by the HUB; `espidi::slot()` — 1 or 2 (0 without the HUB).

A good place for "back to the HUB" is a long press (the template uses the encoder button for 2 s).

## Build, check, install

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3 --library SDK/ESPidiHub --output-dir out MySketch
python3 <ESPidi-HUB>/tools/espidi_hub.py check --require-descriptor out/MySketch.ino.bin
python3 <ESPidi-HUB>/tools/espidi_hub.py install -p /dev/cu.usbmodem1101 --slot 2 out/MySketch.ino.bin
```

`<ESPidi-HUB>` is a clone of the [ESPidi HUB repository](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB); `espidi_hub.py` needs `esptool`.

`check` reports the size, the slot space it takes, the descriptor, the cover and any problem (wrong chip, merged image, too big). `install` sizes the slot, writes the firmware and keeps the other slot, all settings and files intact.

Or without a terminal: the [composer site](https://espidi-hub.github.io/) takes your `.bin` (“My file”) and writes it over USB from Chrome or Edge.

## Publishing

Make your repository **ESPidi-ready** and anyone can install your firmware by pasting its address into the composer site (tab **GitHub**) or with `espidi_hub.py install --slot N owner/repo`. Name, version, cover and description come from the repository; nothing has to be uploaded anywhere else.

An ESPidi-ready repository has `espidi.json` at its root (or in a folder — then link to that folder):

```json
{
  "espidi": 1,
  "sketch": "MyArp",
  "firmware": "espidi/MyArp.ino.bin",
  "cover": "cover.png",
  "summary": { "en": "Arpeggiator with 8 patterns.", "ru": "Арпеджиатор на 8 паттернов." }
}
```

| Field | |
|---|---|
| `espidi` | required, `1` |
| `firmware` | required: the built app image **committed to the repository**, path relative to `espidi.json` |
| `cover` | 64×32 PNG (or a multiple) — [COVERS.md](../SDK/ESPidiHub/COVERS.md) |
| `sketch` | the sketch folder: the workflow builds it, `--sync-cover` writes `<sketch>/cover.h` |
| `summary` | one or two sentences, text or `{"en": …, "ru": …}` |
| `id`, `name`, `version`, `author` | only for firmwares without `ESPIDI_FIRMWARE()`; otherwise the descriptor wins |

Why the `.bin` is in the repository and not only in Releases: a web page cannot download GitHub release files (they come without CORS headers), but it can read files of the repository.

**The easy way:** copy [`SDK/ESPidiTemplate`](../SDK/ESPidiTemplate) into a new repository — it already is an ESPidi-ready repository, with the workflow. On every push the workflow makes `cover.h` from `cover.png`, builds, checks, and **when the version in `ESPIDI_FIRMWARE()` (or the cover) changed, commits the build** to the `firmware` path. So publishing a new version = changing the version string and pushing. On a `v*` tag it also attaches the `.bin` to a release.

**By hand:** build (Arduino IDE: Sketch → Export Compiled Binary), copy `<sketch>.ino.bin` to the `firmware` path, commit, push.

**Check what others will get:**

```bash
python3 <ESPidi-HUB>/tools/espidi_hub.py repo .              # your checkout
python3 <ESPidi-HUB>/tools/espidi_hub.py repo owner/repo     # what the site loads from GitHub
```

**Branches and folders:** `owner/repo@beta` loads the `beta` branch (or a tag, or a commit); a link like `https://github.com/owner/repo/tree/beta/firmware` loads `espidi.json` from the `firmware` folder of `beta`. A link that opens the composer with your firmware in a slot: `https://espidi-hub.github.io/?play=owner/repo` (`page=` for slot 2) — handy for your README.

To be listed in the composer's catalog, open a pull request adding `{"source": "owner/repo", "channel": "stable", …}` to [`web/catalog/catalog.json`](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/web/catalog/catalog.json) in the ESPidi HUB repository.

## ESP-IDF firmwares

The same rules apply. Without the Arduino SDK, add the descriptor by hand:

```c
__attribute__((used, retain, aligned(4))) const struct {
    char magic[12], id[16], name[24], version[16], author[24];
} espidi_firmware_descriptor = {"ESPIDI-FW-1", "myfw", "MY FW", "1.0.0", "Your Name"};

__attribute__((used, retain, aligned(4))) const struct {
    char magic[12];
    unsigned char bits[256];
} espidi_firmware_cover = {"ESPIDI-CV-1", { /* 256 bytes from espidi_hub.py cover */ }};
```

The shared file system is the data partition labelled `spiffs` (LittleFS); NVS is the default `nvs` partition.

## Before you release

- [ ] `check --require-descriptor` passes.
- [ ] Installed next to another firmware: both still start, both keep their settings.
- [ ] Power cycle → HUB; your restart → your firmware; `rebootToHub()` → HUB.
- [ ] Files (if used) live in `/<id>/` and survive a reinstall of the other slot.
- [ ] Nothing in the code formats, erases or does OTA.
- [ ] `repo owner/repo` passes: the composer can load the published version with its cover.

---

# Как написать прошивку для ESPidi HUB

[ESPidi HUB](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB) хранит две прошивки на одной плате ESPidi и при включении запускает одну из них; [сайт-сборщик](https://espidi-hub.github.io/) записывает их на плату из браузера. Здесь всё, что нужно прошивке, чтобы жить в слоте; SDK и шаблон — в [SDK/](../SDK). Для ИИ-агентов те же правила в виде чек-листа — в [AGENTS.md](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/AGENTS.md). English version: [above](#writing-a-firmware-for-espidi-hub).

## Что получает прошивка

- **Слот**: PLAY запускает слот 1, PAGE — слот 2, иначе HUB запускает прошивку, работавшую в прошлый раз.
- **Настройки**: общий NVS (16 КБ), в собственном пространстве имён.
- **Файлы**: общий LittleFS на 1 МБ, в собственной папке.
- **Место**: до 2,62 МБ кода, если второй слот пуст; иначе — сколько осталось рядом со второй прошивкой. Размер выбирать не нужно: его считает установщик.
- **Всю плату**, пока она работает: экран, кнопки, энкодер, MIDI, Wi-Fi/BLE.

Прошивке, которая соблюдает правила ниже, вообще не нужно знать о HUB. RED SEA, например, работает без изменений.

## Быстрый старт (Arduino)

1. Установите ядро ESP32 (Espressif, 3.x) и выберите плату **ESP32C3 Dev Module**.
2. Установите SDK: скопируйте [`SDK/ESPidiHub`](../SDK/ESPidiHub) в папку `libraries` Arduino (или передайте `--library SDK/ESPidiHub` в arduino-cli).
3. Возьмите за основу [`SDK/ESPidiTemplate`](../SDK/ESPidiTemplate).
4. Добавьте в скетч дескриптор — HUB покажет это имя:

```cpp
#include <ESPidiHub.h>
ESPIDI_FIRMWARE("myarp", "MY ARP", "1.0.0", "Your Name");
```

`id` (до 15 символов, строчными) — это ещё и пространство имён в NVS, и имя папки с файлами. `name` и `author` — до 23 символов, `version` — до 15. Слишком длинная строка — ошибка компиляции. Занятые id — эти пространства имён NVS используют HUB и SDK: `espidihub`, `espidi_pad`, `nimble_bond`.

5. По желанию добавьте обложку (ниже) и сделайте репозиторий по SDK (см. «Публикация»): тогда сайт-сборщик ставит прошивку по ссылке.

## Обложка

Когда HUB запускает прошивку, он показывает картинку 64×32 рядом с именем и версией; на сайте-сборщике это миниатюра прошивки. Рисуйте чёрно-белой (белое горит), 64×32 или кратную (128×64, …), без названия, сохраните как `cover.png` в репозитории и укажите в `espidi.json`. `cover.h` для скетча генерируется из неё.

Всё про обложки — как рисовать, откуда они берутся, проверка, формат: **[SDK/ESPidiHub/COVERS.ru.md](../SDK/ESPidiHub/COVERS.ru.md)**.

## Правила

**Обязательно**

1. Собирать под ESP32-C3 обычный образ приложения. Ставится файл `<sketch>.ino.bin` — **не** `*.merged.bin` и не файлы загрузчика или таблицы разделов.
2. Использовать распиновку ESPidi (`espidi::pin::…`).
3. Хранить настройки в NVS в **своём пространстве имён** (`Preferences prefs; prefs.begin("<id>");`).
4. Хранить файлы на общем LittleFS в папке **`/<id>/`**, подключая его через `espidi::mountFiles()`.
5. Укладываться в место, которое показывает установщик (`espidi_hub.py check`).

**Нельзя**

1. Стирать или форматировать общие хранилища: никаких `nvs_flash_erase()`, `LittleFS.format()`, `LittleFS.begin(true)`; `Preferences::clear()` — только для своего пространства имён. Там же лежат настройки и файлы другой прошивки.
2. Использовать SPIFFS или FFat на общем разделе — только LittleFS. Другая файловая система сделает раздел нечитаемым для всех.
3. Использовать OTA-обновления (`Update`, `esp_ota_begin()`, `esp_https_ota()`), вызывать `esp_ota_set_boot_partition()` или писать otadata — выбором слота управляет HUB. Чтобы выйти в HUB, вызовите `espidi::rebootToHub()`.
4. Писать во флеш напрямую или менять таблицу разделов.
5. Рассчитывать, что плата загрузится с зажатым TAP: GPIO2 — strapping-пин.

## Железо

Пины из [HARDWARE.MD](../HARDWARE.MD) — так их называет SDK:

| Сигнал | GPIO | Примечание |
|---|---|---|
| PLAY | 1 | кнопка, `INPUT_PULLUP`, активный LOW |
| TAP | 2 | кнопка; strapping-пин — не должна быть зажата в момент сброса |
| PAGE | 3 | кнопка, на плате подписана L/R |
| Кнопка энкодера | 4 | |
| Энкодер B / A | 5 / 6 | `INPUT_PULLUP` |
| MIDI RX / TX | 7 / 10 | UART, 31250 бод, 8N1 |
| OLED SDA / SCL | 8 / 9 | SSD1306 128×32, I²C-адрес `0x3C`; оба — strapping-пины |

Константы: `espidi::pin::PLAY` … `espidi::pin::MIDI_TX`, `espidi::OLED_ADDR`, `espidi::OLED_WIDTH`, `espidi::OLED_HEIGHT`, `espidi::MIDI_BAUD`.

## Хранение данных

**Настройки** — NVS маленький (16 КБ на всех). Храните несколько значений, а не банки данных:

```cpp
Preferences prefs;
prefs.begin("myarp");            // = id из дескриптора
uint8_t bpm = prefs.getUChar("bpm", 120);
```

**Файлы** — пресеты, паттерны, MIDI-файлы:

```cpp
if (espidi::mountFiles()) {
  String dir = espidi::dataDir("myarp");   // создаёт /myarp
  File f = LittleFS.open(dir + "/preset1.json", "w");
}
```

`mountFiles()` форматирует раздел, только если он полностью пуст (свежая установка). Если раздел не подключается по любой другой причине, функция вернёт `false` и данные не тронет — тогда работайте без файлов.

## Размер

- Установщик округляет образ вверх до 64 КБ и проверяет его по свободному месту.
- Одна на плате: до **2,62 МБ**. Рядом с другой прошивкой: 2,69 МБ минус её размер.
- Arduino IDE со стандартной схемой разделов ограничивает скетч 1,25 МБ. Если ваш больше, выберите **Tools → Partition Scheme → Huge APP**: это снимает только проверку Arduino, разметку HUB это не затрагивает.

## Поведение при запуске

| Событие | Что запускается |
|---|---|
| Включение питания, кнопка RESET | HUB |
| `ESP.restart()` / `esp_restart()` | снова ваша прошивка |
| Падение, watchdog | снова ваша прошивка |
| Выход из deep sleep | ваша прошивка |
| `espidi::rebootToHub()` | HUB |

Дополнительно: `espidi::underHub()` — запущено ли из HUB; `espidi::slot()` — 1 или 2 (0 без HUB).

Удобное место для «выхода в HUB» — долгое нажатие (в шаблоне это кнопка энкодера на 2 секунды).

## Сборка, проверка, установка

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3 --library SDK/ESPidiHub --output-dir out MySketch
python3 <ESPidi-HUB>/tools/espidi_hub.py check --require-descriptor out/MySketch.ino.bin
python3 <ESPidi-HUB>/tools/espidi_hub.py install -p /dev/cu.usbmodem1101 --slot 2 out/MySketch.ino.bin
```

`<ESPidi-HUB>` — клон [репозитория ESPidi HUB](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB); для `espidi_hub.py` нужен `esptool`.

`check` показывает размер, сколько места займёт прошивка, дескриптор, обложку и проблемы (не тот чип, объединённый образ, слишком большой размер). `install` сам рассчитывает слот, записывает прошивку и не трогает второй слот, настройки и файлы.

Или без терминала: [сайт-сборщик](https://espidi-hub.github.io/) принимает ваш `.bin` («Свой файл») и записывает его по USB из Chrome или Edge.

## Публикация

Сделайте репозиторий **по SDK** — и любой поставит вашу прошивку, вставив адрес репозитория на сайте-сборщике (вкладка **GitHub**) или командой `espidi_hub.py install --slot N owner/repo`. Имя, версия, обложка и описание берутся из репозитория; больше никуда ничего загружать не нужно.

В репозитории по SDK в корне лежит `espidi.json` (или в папке — тогда давайте ссылку на эту папку):

```json
{
  "espidi": 1,
  "sketch": "MyArp",
  "firmware": "espidi/MyArp.ino.bin",
  "cover": "cover.png",
  "summary": { "en": "Arpeggiator with 8 patterns.", "ru": "Арпеджиатор на 8 паттернов." }
}
```

| Поле | |
|---|---|
| `espidi` | обязательно, `1` |
| `firmware` | обязательно: собранный образ приложения, **закоммиченный в репозиторий**, путь относительно `espidi.json` |
| `cover` | PNG 64×32 (или кратный) — [COVERS.ru.md](../SDK/ESPidiHub/COVERS.ru.md) |
| `sketch` | папка скетча: её собирает workflow, в неё `--sync-cover` пишет `cover.h` |
| `summary` | одно-два предложения, текст или `{"en": …, "ru": …}` |
| `id`, `name`, `version`, `author` | только для прошивок без `ESPIDI_FIRMWARE()`; иначе главнее дескриптор |

Почему `.bin` лежит в репозитории, а не только в Releases: веб-страница не может скачать файлы релизов GitHub (они отдаются без CORS-заголовков), а файлы репозитория — может.

**Проще всего:** скопируйте [`SDK/ESPidiTemplate`](../SDK/ESPidiTemplate) в новый репозиторий — это уже репозиторий по SDK, вместе с workflow. На каждый push он делает `cover.h` из `cover.png`, собирает, проверяет и, **если версия в `ESPIDI_FIRMWARE()` (или обложка) изменилась, коммитит сборку** по пути `firmware`. То есть выпустить новую версию = поменять строку версии и запушить. На тег `v*` он ещё и прикладывает `.bin` к релизу.

**Вручную:** соберите (Arduino IDE: Скетч → Экспорт бинарного файла), скопируйте `<sketch>.ino.bin` по пути `firmware`, закоммитьте, запушьте.

**Проверить, что получат другие:**

```bash
python3 <ESPidi-HUB>/tools/espidi_hub.py repo .              # ваша локальная копия
python3 <ESPidi-HUB>/tools/espidi_hub.py repo owner/repo     # то, что сайт загрузит с GitHub
```

**Ветки и папки:** `owner/repo@beta` загрузит ветку `beta` (или тег, или коммит); ссылка вида `https://github.com/owner/repo/tree/beta/firmware` — `espidi.json` из папки `firmware` ветки `beta`. Ссылка, открывающая сборщик сразу с вашей прошивкой в слоте: `https://espidi-hub.github.io/?play=owner/repo` (`page=` — для слота 2); удобно поставить в README.

Чтобы попасть в каталог сборщика, пришлите pull request с записью `{"source": "owner/repo", "channel": "stable", …}` в [`web/catalog/catalog.json`](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/web/catalog/catalog.json) репозитория ESPidi HUB.

## Прошивки на ESP-IDF

Правила те же. Без Arduino-SDK дескриптор добавляется вручную:

```c
__attribute__((used, retain, aligned(4))) const struct {
    char magic[12], id[16], name[24], version[16], author[24];
} espidi_firmware_descriptor = {"ESPIDI-FW-1", "myfw", "MY FW", "1.0.0", "Your Name"};

__attribute__((used, retain, aligned(4))) const struct {
    char magic[12];
    unsigned char bits[256];
} espidi_firmware_cover = {"ESPIDI-CV-1", { /* 256 байт из espidi_hub.py cover */ }};
```

Общая файловая система — data-раздел с меткой `spiffs` (LittleFS); NVS — стандартный раздел `nvs`.

## Перед релизом

- [ ] `check --require-descriptor` проходит.
- [ ] Рядом с другой прошивкой обе запускаются и обе сохраняют настройки.
- [ ] Выключение/включение → HUB; ваш перезапуск → ваша прошивка; `rebootToHub()` → HUB.
- [ ] Файлы (если есть) лежат в `/<id>/` и переживают переустановку соседнего слота.
- [ ] В коде нет форматирования, стирания и OTA.
- [ ] `repo owner/repo` проходит: сайт загружает опубликованную версию с обложкой.
