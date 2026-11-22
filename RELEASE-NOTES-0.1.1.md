# Big Red Racing fixes 0.1.1

A fix release for **Windows 95, 98 and ME**. Behaviour under modern Windows / Wine / Proton is unchanged, as is the DOS patcher (which remains at version 0.1.0).

**This is still a testing release:** please report anything that doesn't work in [Issues](https://github.com/reeshub/brrfix/issues).

| Download | For | Version |
|---|---|---|
| `big-red-launcher-0.1.1.zip` | The Windows version, on Windows 95 to 11, Linux and the Steam Deck (Proton) | Big Red Launcher 0.1.1 |
| `brr-dos-patch-0.1.0.zip` | The DOS version, on MS-DOS 6 and later, Windows 9x's MS-DOS Prompt and DOSBox | DOS patch 0.1.0 (unchanged) |

Neither download contains any part of the game: both work on your own copy, patched to v2.25 with the official update.

## Big Red Launcher 0.1.1

### Windows 95, 98 and ME

- **Fix erroneous cnc-ddraw behaviour.** Release 0.1.0 bundled the cnc-ddraw wrapper for modern Windows in such a way that Windows 9x was also attempting to load it, leading to a "CNCDD.DLL expects a newer version of Windows" error. The launcher now uses cnc-ddraw only on Windows XP onwards and under Wine and Proton; on 95, 98 and ME the game uses Windows' own DirectDraw, as it did in 1996. If you already have 0.1.0 there, start `Big Red Racing.exe` once after updating: it rebuilds `WINRACEP.EXE` without cnc-ddraw.
- **Ripped CD music now plays.** The music player never started on Windows 95, 98 and ME, whatever the format: those versions of Windows refuse to start a program thread without somewhere to write its ID in memory, which newer Windows (and Wine) don't mind.
- **Game timing correction helper now loads.** Big Red Launcher's game timing patch was also affected by the above, so that has also now been fixed.
- **Music uses less memory.** FLAC and WAV tracks are now streamed from the disk as they play, instead of each track being loaded into RAM when it starts (up to 45 MB for a WAV track, a lot for a 1990s PC). Incidentally, when running on a period PC, WAV is recommended as it is the lightest format CPU-wise.
- **8 sound channels by default**, as in 1996: Big Red Launcher can expand the in-game sound channels from 8 to 32. This eats up a lot of CPU cycles. Therefore this release defaults to the original 8 on legacy Windows. This can still be expanded in the launcer if your PC can handle it.
- **Windowed / full screen toggle fixed.** This original game behaviour was patched out completely in 0.1.0 as it causes problems with the cnc-ddraw wrapper for modern Windows. It's now been reinstated when running under legacy Windows. Toggle using TAB or the option in the original game window. Note that games started in fullscreen can't be toggled back to windowed - this was the original behaviour in 1996. When running under modern Windows, Alt-Enter works as expected.
- **Big Red Launcher visual re-jig.** Removed the "Modern defaults" button, and also tidied up tab layour and window sizing to take into account the hidden options when running under legacy Windows.
- **Clearer messaging in installer batch file.** The included `Install Game Files.bat` relies on PowerShell for CD audio ripping and other functionality, making it incompatible with legacy Windows. In 0.1.0 is was throwing an unhelpful error - this release adds Windows version detection and a more helpful message for players attempting to run it under Legacy Windows.

### Other bits and bobs

- Help file and `README.txt` updated to reflect the new changes.

Everything else is as in [0.1.0](https://github.com/reeshub/brrfix/releases/tag/v0.1.0), whose notes list all of the fixes.

## DOS patch 0.1.0

Unchanged. Version 0.1.0 as before.

## Updating from 0.1.0

Unzip `big-red-launcher-0.1.1.zip` over your Big Red Launcher folder: your `Game` and `Music` folders stay as they are. Windows will ask whether to replace `BRRFIX.INI`, which holds your saved settings: replace it to start from 0.1.1's defaults (including 8 sound channels on Windows 95, 98 and ME), or keep yours to keep your settings.

## What you need

Both fixes only support the v2.25 patched version of the game, the last (and only) official update. The easiest way to get it is by downloading the two zips from [myabandonware.com](https://www.myabandonware.com/game/big-red-racing-2n8). You'll need the "Disc Image" rather than the "RIP Version", as the disc image includes the original CD audio tracks:

- `Big-Red-Racing_DOS_EN_Disc-Image.zip` (the original CD)
- `Big-Red-Racing_Patch_DOS_EN_Patch-v2-25.zip` (the official v2.25 patch)

Both fixes check the game's files and refuse any other version, changing nothing:

| File | Size (bytes) | SHA-256 |
|---|---|---|
| `WINRACE.EXE` (Windows) | 534,016 | `c0274f1248d552f09310a489cc34bc04a7e03f9485f40d24549b3a729752ecaa` |
| `RACING.EXE` (DOS) | 629,472 | `5186af0605ea87dfb8aee3463ff24acc259118fab611b07ec6e56b9f46c52636` |

## Installing

- **Windows 10 and 11:** unzip `big-red-launcher-0.1.1.zip` into a folder of its own, add the two zips, run `Install Game Files.bat`, then start `Big Red Racing.exe`.
- **Windows 95, 98 and ME:** set up the folder on a newer PC with `Install Game Files.bat` and copy it across, or copy your own installed game into its `Game` folder. You can leave out `CNCDD.DLL`, `ddraw.ini`, `Shaders` and `DPLAY.DLL`: those versions' own DirectX draws the game and has DirectPlay. Start the game with `Big Red Racing.exe`, not `WINRACEP.EXE`.
- **Steam Deck and Linux:** set up the folder on a Windows PC and copy it across (or install the game yourself into its `Game` folder), then add `Big Red Racing.exe` to Steam as a non-Steam game and choose a Proton version.
- **DOS:** copy `BRRPATCH.EXE` next to `RACING.EXE` and run `BRRPATCH`.

The full details are in each zip's `README.txt` / `README.TXT`, and the launcher's `Big Red Launcher Help.txt`.

## Known issues

- Windows' SmartScreen may warn about `Big Red Racing.exe` and `Install Game Files.bat` the first time, as they aren't signed: choose "More info", then "Run anyway".
- `Install Game Files.bat` needs Windows PowerShell 5.1 (Windows 10 and 11), so it doesn't run under Linux, Proton or Windows 95, 98 and ME.
- Split-screen races aren't drawn by the Modern renderer yet: they keep the game's own 640×480 picture and dashboard.
- The game draws in software: very tall resolutions and the furthest draw distances cost frame rate on slower PCs (lower "Max limit" or the draw distance if so).
- Some objects can still flicker in front of things behind them now and then, an engine limitation only partly fixed so far.
- It's not possible to fully navigate the menus using joystick input yet. Keep a keyboard and mouse handy for now.

## Thanks

- Paul and Peter Ranson - and everyone else at Big Red Software!
- cnc-ddraw by FunkyFr3sh; dr_flac by David Reid; stb_vorbis by Sean Barrett.
- FLAC (Xiph.Org) and FFmpeg (builds by gyan.dev), which the installer can download to convert the music.

Big Red Launcher and the DOS patch are free software under the GNU General Public License, version 3 or later. *Big Red Racing* itself is not included, and remains the property of its rights holders.
