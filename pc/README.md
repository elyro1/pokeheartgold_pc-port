# Native PC port

See [INSTALL.md](../INSTALL.md) for build and run instructions.

Autosave is enabled by default and archives progress every two minutes of real
time. During dialogue, menus, battles, map transitions and special activities,
it waits for normal overworld control and a tile boundary.

Snapshots are stored in an `autosave/` folder beside the active save file, for
example `autosave/save1.bin.2026-11-04_14-30-00-0000.bak`. The newest ten snapshots
are kept independently for each slot. Autosaves preserve the manual save file
and its live save bookkeeping; failed writes preserve existing backups.

To restore a snapshot, close the game, back up the corresponding manual save,
then copy the snapshot over `save1.bin`, `save2.bin` or `save3.bin` in the game's
run folder. Autosaves are restored manually; Continue loads the normal slot.

Press **Tab** to open PC settings and toggle **Autosave** in the
**HeartGold / SoulSilver** window. The setting is stored as `Autosave=true` or
`Autosave=false` in the `[Developer]` section of `sim_config_prj.ini`.

