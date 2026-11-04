# PC port — build & run

The PC target supports **Linux x86-64** and a native **Windows x64** build using MSYS2 UCRT64. WSL2 + WSLg remains an alternative for running the Linux version on Windows.

The PC build uses the open-source replacement libraries and does not need MWCC, Wine, or the proprietary Nitro SDK.

## Download a CI build

The `linux-HEARTGOLD` and `windows-HEARTGOLD` CI artifacts are ZIP files containing
the executables, runtime libraries, a game-specific `header.bin`, blank Wi-Fi
settings in `firmware.bin`, and a README. Extract the whole ZIP to a writable
folder. There is no inner archive.

On Windows, double-click `hg-game.exe`. On Linux, run `hg-game` directly (or
`./hg-game` from a terminal); bundled libraries are found automatically in `lib/`.
If your ZIP extractor removes executable permissions, mark `hg-game` and
`hg-extract` executable in file properties, or run `chmod +x hg-game hg-extract`
once. The Linux artifact targets Ubuntu 24.04 or newer (glibc 2.39+) and needs
your system's OpenGL 4.2 graphics driver. The game finds its runtime folder
regardless of the working directory.

On first launch, accept the extraction prompt and choose your unmodified US
HeartGold revision 0 ROM. Game assets are extracted into the package folder;
no ROM or game assets are included in CI. The included README also explains
command-line extraction with `hg-extract`. If startup fails, check `DbgLog.txt`
or run the executable from a terminal to see startup errors.
Preserve saves, extracted assets, configuration, and your existing `firmware.bin`
when updating a downloaded build.

## Linux

### 1. Install dependencies

Use a 64-bit Linux desktop with OpenGL 4.2 support, Meson **1.3+**, Python **3.9+**, and a C++20 compiler/library with `std::format` support (GCC/G++ 13 or newer).

On **Ubuntu 24.04**, install:

```bash
sudo apt update
sudo apt install build-essential git python3 meson ninja-build nasm \
    flex bison pkg-config libsdl2-dev libenet-dev libsoundtouch-dev
```

Other distributions need the equivalent packages. The first configuration downloads the SDK subprojects, so it needs internet access.

### 2. Clone

```bash
git clone https://github.com/ctrl-cheeb-del/pokeheartgold.git
cd pokeheartgold
```

Already have a checkout? Run the remaining commands from its root.

### 3. Build HeartGold

```bash
make -f Makefile.pc linux JOBS=4
```

Executable: `build/pc/heartgold/hg-game`.

`JOBS=4` limits parallel compilation. Increase it for faster builds or lower it if you run out of RAM.

### 4. Extract your ROM's assets

Replace the example path with your own dumped US HeartGold ROM:

```bash
make -f Makefile.pc assets ROM="/absolute/path/to/HeartGold.nds"
```

This checks the ROM's revision and SHA1, then extracts assets into `build/pc/heartgold/run/`. Run it once per game, or again if the extracted files need replacing. Existing saves and configuration are retained.

### 5. Play

```bash
cd build/pc/heartgold/run
../hg-game
```

The executable automatically uses its adjacent `run/` folder when it contains extracted assets. Press **Tab** for PC settings.

## SoulSilver

From the repository root, select SoulSilver for **both** build and extraction:

```bash
make -f Makefile.pc linux GAME_VERSION=SOULSILVER JOBS=4
make -f Makefile.pc assets GAME_VERSION=SOULSILVER ROM="/absolute/path/to/SoulSilver.nds"
cd build/pc/soulsilver/run
../hg-game
```

Each game has its own build and run folders. No clean is needed when switching between them.

## Windows (native x64)

Install [MSYS2](https://www.msys2.org/), open its **UCRT64** terminal, and update it:

```bash
pacman -Syu
```

If the update closes the terminal, reopen **MSYS2 UCRT64** and run the command again. Then install the build dependencies:

```bash
pacman -S --needed git make flex bison \
    mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python \
    mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-nasm mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-enet \
    mingw-w64-ucrt-x86_64-soundtouch
```

Clone the repository as above, then build from its root in the UCRT64 terminal:

```bash
make -f Makefile.pc win64 JOBS=4
make -f Makefile.pc assets PC_PLATFORM=win64 ROM="C:/Games/HeartGold.nds"
cd build/pc/win64/heartgold/run
../hg-game.exe
```

This creates `build/pc/win64/heartgold/hg-game.exe` and `hg-extract.exe`, and copies their required MinGW DLLs beside them. Keep those DLLs with the executables when moving the build to another Windows computer. Extracted assets, saves, and settings live in `run/`; the executable finds this folder automatically. You can double-click `hg-game.exe` or use it as a Windows shortcut target.

For SoulSilver, add `GAME_VERSION=SOULSILVER` to both make commands and use `build/pc/win64/soulsilver/run`. Windows and Linux builds have separate folders. Rebuild with the same `win64` command after source changes.

Use the UCRT64 toolchain consistently; MSVC and 32-bit Windows builds are not supported. The Windows CI workflow stages a HeartGold runtime package with DLLs and startup files, without needing a ROM. See **Download a CI build** above for using the downloaded artifact.

## Windows (WSL2)

This builds a Linux executable and runs its window through WSLg. It does not produce an `.exe`.

### 1. Set up Ubuntu

In **PowerShell as Administrator**:

```powershell
wsl --install -d Ubuntu-24.04
wsl --update
```

Restart if prompted, then open **Ubuntu 24.04** and finish creating your Linux user. Ensure the distribution uses WSL2 (`wsl --list --verbose` in PowerShell).

GUI apps require Windows 10 build 19044+ or Windows 11, WSL2, and a compatible graphics driver. See Microsoft's [WSL installation guide](https://learn.microsoft.com/en-us/windows/wsl/install) and [GUI app setup](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).

### 2. Build inside Ubuntu

Follow the **Linux** steps above in the Ubuntu terminal. Clone under your Linux home directory, for example `~/pokeheartgold`.

You can reference a ROM stored on Windows with its WSL path:

```bash
make -f Makefile.pc assets ROM="/mnt/c/Users/YourName/Games/HeartGold.nds"
cd build/pc/heartgold/run
../hg-game
```

The game window should open on your Windows desktop through WSLg. Saves stay in the Linux `run/` folder. Add `GAME_VERSION=SOULSILVER` and use the SoulSilver paths when building that version.

## Quick fixes

- **ROM rejected?** Use an unmodified US revision 0 dump of the selected game. Other regions, revisions, and patched ROMs fail validation.
- **Missing assets?** Run the `assets` command; the executable automatically finds that game's `run/` directory.
- **Missing `<format>` / `std::format`?** Check your C++ compiler and standard library; use GCC/G++ 13+.
- **Build failed?** Compiler/linker output is saved to `build/pc/<game>/game-link.log`.
- **Windows DLL missing?** Re-run `make -f Makefile.pc win64` from the UCRT64 terminal to stage runtime DLLs. Keep all generated DLLs beside `hg-game.exe`.
- **WSL window won't open?** Check WSLg, update WSL and your Windows GPU driver, then retry. This port's WSL runtime remains untested here.

Accepted ROM SHA1s:

| Game | US code | SHA1 |
| --- | --- | --- |
| HeartGold | IPKE | `4fcded0e2713dc03929845de631d0932ea2b5a37` |
| SoulSilver | IPGE | `f8dc38ea20c17541a43b58c5e6d18c1732c7e582` |

To rebuild after source changes, return to the repository root and rerun the relevant `make -f Makefile.pc linux` command. You do not need to extract the ROM again.
