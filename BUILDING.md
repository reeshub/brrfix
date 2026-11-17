# Building Big Red Launcher and BRRPATCH

`build.py` builds both from this repository's source and packages them exactly as they're released.

## What you need

- **Linux on x86-64.** The build uses Open Watcom's Linux tools.
- **Python 3.10 or later.** Nothing else from Python: the scripts use its standard library only.
- **Open Watcom v2**, the 2026-11-01 snapshot. `python3 build.py --get-watcom` downloads it (150 MB) from the project's [official release](https://github.com/open-watcom/open-watcom-v2/releases/tag/2026-11-01-Build) into `tools/open-watcom/`, checking its SHA-256 (`89e90b7544cf3ea4a38124ec4c104a5027f31101405d7115c3f833daa465bb48`).
- **Internet access** during the build, for three open-source parts, each pinned and checked by SHA-256: [dr_flac](https://github.com/mackron/dr_libs) 0.13.4 and [stb_vorbis](https://github.com/nothings/stb) (commit `4e9b368`), compiled into `PATCH.DLL`, and [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw) v7.1.0.0, which goes in the launcher's zip. They're saved in `tools/`.
- **Your own copy of the game, patched to v2.25:** the `Game` folder that `Install Game Files.bat` makes, for example. `build.py` checks that its `WINRACE.EXE`, `RACING.EXE` and `TIMER.DLL` are v2.25's (`RACING.EXE` not yet patched by the DOS patch).

## Build

```sh
python3 build.py --get-watcom
python3 build.py "/path/to/Big Red Racing/Game"
```

The results:

| Where | What |
|---|---|
| `work/build/dropin/` | `PATCH.DLL`, `DPLAY.DLL`, `Big Red Racing.exe` (the launcher), `Install Game Files.bat` |
| `work/build/brrpatch/` | `BRRPATCH.EXE` |
| `work/release/` | `big-red-launcher-<version>.zip` and `brr-dos-patch-<version>.zip`, as released |

## Why the build needs the game

Nothing of the game goes into what the build makes, and both packaging steps refuse any file whose checksum matches one of the game's. But the build checks its work against your copy:

- `PATCH.DLL` changes the game's code only in memory, at places listed in `data/dropin-patches.json`. The build checks every one of them against your `WINRACE.EXE`.
- The launcher's icon is the game's own, taken from your `WINRACE.EXE`.
- BRRPATCH and the launcher's "Patch the DOS version" button carry the steps that rebuild the patched `RACING.EXE` from a player's own copy. They're worked out from your `RACING.EXE` and `data/dos-patches.json`.

## What's where

| Folder | What |
|---|---|
| `src/dropin/` | `PATCH.DLL`, the fix itself (C and assembly), with `BRRFIX.INI`, `ddraw.ini` and the zip's `README.txt` |
| `src/launcher/` | `Big Red Racing.exe`, the launcher and its settings window, and its help (`HELP.md`) |
| `src/setup/` | `Install Game Files.bat`, filled in with the facts in `data/` |
| `src/brrpatch/` | `BRRPATCH.EXE`, the DOS patcher |
| `src/dospatch/` | The DOS patch's source, and the DOS zip's `README.txt` |
| `data/` | The patch sites and the DOS patch's compiled form, with checksums; facts about the game's files, the zips and the CD's music |
| `scripts/` | The build and packaging scripts that `build.py` runs |

`data/dos-patches.json` is the DOS patch compiled from `src/dospatch/`: our own code, where it goes in `RACING.EXE`, and checksums. BRRPATCH, the launcher's DOS button and the Python patcher (`scripts/dos_patch.py`) apply it as it is. Compiling it again takes tools that aren't in this repository.

The version numbers are in `src/dropin/version.h` (Big Red Launcher) and `data/dos-patches.json` (the DOS patch).
