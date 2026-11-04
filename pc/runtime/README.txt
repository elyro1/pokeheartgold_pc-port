HeartGold / SoulSilver native PC runtime (@GAME@)

Extract the entire downloaded ZIP into a writable folder before starting.
Keep header.bin, firmware.bin, the executables and libraries together.

Windows: double-click hg-game.exe.
Linux: run hg-game directly (or ./hg-game from a terminal).
Linux needs glibc 2.39+ (Ubuntu 24.04 or newer) and an OpenGL 4.2 graphics driver.
Linux loads bundled libraries from lib/ automatically. Windows DLLs live beside
hg-game.exe. If your ZIP extractor removes Linux executable permissions, mark
hg-game and hg-extract executable in file properties, or run:
  chmod +x hg-game hg-extract

On the first launch, accept the ROM extraction prompt and select your own
unmodified US revision 0 ROM for @GAME@. The runtime extracts the game files
into this folder. No ROM, game assets, or dumped DS firmware are included.
header.bin supplies a game-specific startup header. firmware.bin starts as
blank writable Wi-Fi settings.

You can also extract from a terminal in this folder:
  Windows: hg-extract.exe "C:\Games\@GAME@.nds"
  Linux: ./hg-extract "/path/to/@GAME@.nds"

The executables find their runtime folder automatically, regardless of the
working directory. To extract elsewhere, use hg-extract --output DIRECTORY ROM.nds.
Saves, autosave/, extracted assets, and configuration are kept in this folder.
Keep your existing firmware.bin and saves when updating to a new build.
If startup fails, check DbgLog.txt or run the executable from a terminal to see
startup errors.
