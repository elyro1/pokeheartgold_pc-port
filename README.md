# Pokémon HeartGold / SoulSilver — PC Port

Johto, on PC. A work-in-progress native port built from [ctrl-cheeb-del's decompilation](https://github.com/ctrl-cheeb-del/pokeheartgold), powered by [libntr](https://github.com/cybervisi0n/libntr).

> **Warning: this port is experimental** Save often. If a crash costs you progress, you can restore an available backup from the `autosave/` folder beside your save files. Close the game, keep a copy of your manual save, then copy the backup over its matching save file (for example, `save1.bin`), removing the timestamp and `.bak` suffix. See [autosave and restore instructions](pc/README.md).

**Bring your own dumped US HeartGold or SoulSilver ROM (revision 0).** Assets are extracted locally. No ROMs or game assets are provided.

## Start here

**[Build & run: Linux / Windows](INSTALL.md)**

- **Linux x86-64:** native build.
- **Windows x86-64:** native `.exe` build with MSYS2 UCRT64, or a Linux build through WSL2.

## Features

- **HeartGold + SoulSilver** — choose either at build time.
- **30 / 60 FPS** — 60 FPS enabled by default.
- **PC controls** — keyboard, controller, mouse touch input, fullscreen, fast-forward.
- **Autosave backups** — every two minutes when safe; keeps the newest ten per slot.

## Controls, quickly

| Key | Action |
| --- | --- |
| Arrow keys | Move |
| Z / X | Confirm / back (DS A / B) |
| C / V | Menu / registered item (DS X / Y) |
| Q / E | L / R |
| Enter / Right Shift | Start / Select |
| **Tab** | **PC settings & tools** |
| Hold Left Shift | Fast-forward |
| F11 | Fullscreen |
| Left mouse button | Touch input |

Remap controls in the PC settings. Existing configs keep your saved bindings.
