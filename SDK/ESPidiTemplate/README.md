# Arduino firmware template

An ESPidi-ready repository: paste its address into the [ESPidi composer](https://espidi-hub.github.io/) (tab **GitHub**) and the firmware arrives with its name, cover and description.

```
espidi.json                      what the composer reads: sketch, built firmware, cover, summary
cover.png                        64×32 cover (or 128×64, …), white = lit
espidi/ESPidiTemplate.ino.bin    the published build (the workflow commits it after the first push)
ESPidiTemplate/
  ESPidiTemplate.ino             the sketch, with ESPIDI_FIRMWARE(...)
  cover.h                        generated from cover.png
.github/workflows/firmware.yml   build, check, publish
```

1. Copy this folder into a new repository (its root becomes the repository root).
2. Rename `ESPidiTemplate/` and `ESPidiTemplate.ino` to your sketch name; change `"sketch"` and `"firmware"` in `espidi.json` to match.
3. Change `ESPIDI_FIRMWARE("template", "TEMPLATE", "0.1.0", "Your Name")` — the id is also your NVS namespace and folder.
4. Draw your `cover.png` ([covers](../ESPidiHub/COVERS.md)), write your `"summary"`.
5. Push. The workflow builds and, when the version in `ESPIDI_FIRMWARE()` is new, commits the build to `espidi/`. Without the workflow: build in Arduino IDE (Sketch → Export Compiled Binary), copy `<sketch>.ino.bin` to the `"firmware"` path, commit.
6. Check what the composer will see: `python3 <ESPidi-HUB>/tools/espidi_hub.py repo <owner>/<repo>`.

Local builds need the SDK ([`SDK/ESPidiHub`](../ESPidiHub)) in your Arduino `libraries` folder. After changing `cover.png`: `python3 <ESPidi-HUB>/tools/espidi_hub.py repo . --sync-cover` (see [SDK/README.md](../README.md)).

Rules: [docs/FIRMWARE.md](../../docs/FIRMWARE.md) · for agents: [AGENTS.md](https://github.com/Cirocular-Vlasenko-Daniil/ESPidi-HUB/blob/main/AGENTS.md).
