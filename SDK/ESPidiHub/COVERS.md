# Covers

Русская версия: [COVERS.ru.md](COVERS.ru.md).

A cover is the 64×32 picture the ESPidi HUB shows while it starts your firmware, and the thumbnail of your firmware in the composer site.

```
 ┌────────────────────────────────┬────────────────────────────────┐
 │                                │ MY ARP                         │
 │          cover, 64×32          │ 1.2.0                          │   OLED 128×32
 │                                │                                │
 └────────────────────────────────┴────────────────────────────────┘
```

The HUB prints the name (up to two lines of 10 characters) and the version on the right. Without a cover it shows the name alone.

## Drawing it

- **64×32 pixels, black and white. White = lit.** Transparent counts as black.
- A multiple works too — 128×64, 192×96, … Every 2×2, 3×3, … block becomes one pixel, lit if at least half of it is white. Handy when your editor can't zoom well.
- No firmware name in the picture: the HUB prints it next to the cover. A picture, a logo, a glyph — something you recognise in half a second.
- The OLED has no grey. Use dithering (checkerboards, dotted lines) for shades and test the result on the composer's preview.
- Black-on-white art: convert with `--invert` instead of redrawing.
- Already have an Adafruit GFX `drawBitmap()` array of a 64×32 picture (image2cpp: *Horizontal – 1 bit per pixel*)? It is exactly the cover format.

## Where it lives

In an ESPidi-ready repository (see [FIRMWARE_GUIDE — Publishing](../../docs/FIRMWARE.md#publishing)) the cover is **one file in the repository**, named in `espidi.json`:

```json
{ "espidi": 1, "sketch": "MyArp", "firmware": "espidi/MyArp.ino.bin", "cover": "cover.png" }
```

Everything else follows from that file:

| Who | Takes the cover from |
|---|---|
| composer site, tab **GitHub** | `cover.png` of the repository, from the same commit as the firmware |
| `espidi_hub.py install --slot N owner/repo` | the same |
| the firmware itself | `<sketch>/cover.h`, generated from `cover.png` |
| the HUB on the board | what the installer stored next to the slot; else the cover inside the firmware; else the name only |

Users can still replace or remove a cover when they install (site: Upload / Remove; CLI: `--cover`, `--no-cover`).

## Putting it into the firmware

```bash
python3 <ESPidi-HUB>/tools/espidi_hub.py repo . --sync-cover   # writes <sketch>/cover.h from espidi.json
```

```cpp
#include <ESPidiHub.h>
#include "cover.h"   // ESPIDI_COVER(...) — 256 bytes, generated, don't edit
```

The template's workflow runs `--sync-cover` before every build, so the published firmware always carries the repository's cover; locally, run it after changing `cover.png`. Without a repository: `espidi_hub.py cover picture.png > MySketch/cover.h`.

The cover costs 268 bytes of flash. Without `cover.h` the firmware simply has no built-in cover.

## Checking

```bash
python3 <ESPidi-HUB>/tools/espidi_hub.py repo .               # local checkout
python3 <ESPidi-HUB>/tools/espidi_hub.py repo owner/repo      # what the composer will see
```

It fails on a missing or wrongly sized cover and warns when the cover inside the published firmware differs from `cover.png`.

## Format

1 bit per pixel, rows top to bottom, 8 bytes per row, most significant bit = leftmost pixel, 1 = lit — 256 bytes. Inside a firmware: `ESPIDI-CV-1\0` + the 256 bytes (`ESPIDI_COVER`). Stored for the HUB in the `espidi` partition. Byte layouts: [docs/ARCHITECTURE.md](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/docs/ARCHITECTURE.md).
