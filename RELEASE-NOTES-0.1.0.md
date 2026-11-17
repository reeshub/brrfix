# Big Red Racing fixes 0.1.0

The first release of two free fixes for *Big Red Racing* (Big Red Software / Domark, 1996). **This is a testing release:** please report anything that doesn't work in [Issues](https://github.com/reeshub/brrfix/issues).

| Download | For | Version |
|---|---|---|
| `big-red-launcher-0.1.0.zip` | The Windows version, on Windows 95 to 11, Linux and the Steam Deck (Proton) | Big Red Launcher 0.1.0 |
| `brr-dos-patch-0.1.0.zip` | The DOS version, on MS-DOS 6 and later, Windows 9x's MS-DOS Prompt and DOSBox | DOS patch 0.1.0 |

Neither download contains any part of the game: both work on your own copy, patched to v2.25 with the official update.

## Big Red Launcher 0.1.0

A launcher (`Big Red Racing.exe`), a fix the game loads in place of its own timer (`PATCH.DLL`), and an installer for the game files. Your game's own files are left as they are: the launcher runs its own corrected copy of `WINRACE.EXE`, and changes the game's code only in memory, after checking that every place it changes is exactly as expected. If anything doesn't match, it changes nothing and says why in `BRRFIX.LOG`.

### Getting started

- **Install Game Files.bat** (Windows 10 and 11): put the game's two zips from myabandonware.com next to it and double-click it. It installs the game and the v2.25 patch into a `Game` folder (checking every one of its 866 files), converts the CD music into a `Music` folder as FLAC, Ogg Vorbis or WAV (downloading the official FLAC tool or FFmpeg if needed), and can make a desktop shortcut. Run it again at any time to check the game and put back any missing or damaged files.
- **A settings window** with help for every setting (hover over for tooltips, or click the Help button), and "Period defaults" (1990s PCs) and "Modern defaults" buttons. It can be skipped (`-play`, or "Skip this window next time"; hold Shift or use `-setup` to see it again), and comes back when the game ends, so Steam counts your play time.

### Runs on modern Windows

- Starts on Windows 10 and 11, which refused the original: its file headers are corrected in the launcher's copy, `WINRACEP.EXE`.
- No more "privileged instruction" crash: the game's direct reads of the 1996 VGA card are replaced.
- A stand-in for DirectX 3's DirectPlay, which Windows 10 and 11 lack (the game wouldn't start without it).
- cnc-ddraw draws the game, loaded by its own name, so Wine and Proton can't swap in their own DirectDraw: no launch options or DLL overrides needed.
- Full screen at your desktop's own resolution, or a plain window (Alt+Enter switches). The game's own full-screen switch (Tab, and its menu), which broke the window, is removed.
- Windows 95, 98 and ME still work, with the Original renderer and their own DirectX.

### Smooth at any frame rate

- A steady clock at 70 ticks per second, as per the DOS version (the 1996 Windows version ran 2% fast; now offered as an option under "Advanced").
- The vehicles and the follow camera are drawn between the game's 35 physics steps a second, so movement is smooth at any refresh rate, with a configurable frame-rate cap and optional vsync.
- No more camera stutter: the engine could only tilt and turn the view in steps of 0.35°, so the picture jolted as the camera moved and its lean flicked; races are now drawn with finer angles and a steady lean.
- Two camera calculations that depended on the frame rate have now been decoupled.
- The race start's sudden camera twitch is gone (the mouse is held still for the first second), and the track screen's helicopter flyover (View) is smooth too.
- **The physics is exactly as it was in 1996 - by design**.

### The Modern renderer

The Original renderer preserves the 1996 experience; the Modern one (default on modern systems) adds:

- **Races at your screen's resolution** The original Virtek 3D engine's old hardcoded 320x200 and 640x480 resolutions - and indeed its internal 1,008×756 resolution limit - have been lifted. The game is rendered at half your screen's resolution (by default, this is configurable) and integer scaled to preserve the original sharp pixelated look. The original 3D maths has been upgraded from 32 to 64 bits, as its 32 bits overflowed at today's resolutions, resulting in random spikes and broken objects.
- **Optional native widescreen:** 16:10 and 16:9 tested, wider should work too(!?) Importantly, nothing is distorted - menus stay 4:3 with extended backgrounds, the globe on the track screen stays round, and the game's original Virtek 3D engine has also been patched to render a native widescreen picture. A **huge** amount of hand-tweaking has gone into this particular feature to get it all looking right, and I'm particularly proud of it!
- **In-game HUD** Now rendered in a new layer independently of game resolution. The original "high resolution" graphics were soft, so this uses the low res artwork in all resolutions, integer scaled for sharpness. There's also a new "small" option, so the HUD isn't enormous at modern resolutions.
- **Full colour:** the sky as a smooth gradient instead of 16 bands, and lit surfaces (buildings, trees, vehicles, tunnel walls) shaded smoothly.
- **Far draw distance:** The ground, scenery and vehicles drawn up to 60 squares away (the game managed 22), with the texture distance chosen separately for performance.
- **Consistent look:** Every model is now rendered at its most detailed (as modelled in 1996), so fences, barriers, and other in-game objects no longer change shape close up. Lighting is now consistent for objects at every distance, ground textures are drawn at their highest quality at every distance (so the ground doesn't "wobble" as you drive over it), z-fighting (mostly) fixed by pre-sorting objects, and the gaps between level chunks have been tightened, so less sky shows through tunnels and between billboards (WIP).
- Scaling choices (square or 4:3; Sharp, Nearest, Smooth, Integer or Stretch), a window size in whole multiples of 640×480, and 256 colours for those who want the original palette.

### Sound and music

- **Original CD music** from ripped tracks (FLAC, Ogg Vorbis or 16-bit WAV) in the `Music` folder, with the game's own CD volume option now fixed.
- **Distant vehicles quieter:** The game's engine fades competitor vehicles based on distance from the player, but the Windows version passed DirectSound its volumes in the wrong units, so everything played within about 1 dB of full volume. It fades as the DOS version does, and the game's effects volume option works properly.
- **Natural stereo:** Competitor vehicle and other environmental sounds now pan smoothly between the speakers, whereas the original put anything slightly off centre almost entirely in one. At max volume.
- **Up to 32 sound channels** (expanded from the original 8), with every vehicle's engine and other sound effects heard (expanded from the original closest 3).

### Modern controller support

- Modern pads (XInput) are now fully supported for players one and two, with deadzone and sensitivity adjustment. The game's original "forward to accelerate, back to brake" can also be remapped to use the triggers. Start pauses a race and Back ends it.
- Vibration when your vehicle takes damage, and a rumble while the nitro burns.
- Menu navigation with a pad or joystick: Button 1 (A) to proceed, Button 2 (B) to go back, left and right on the stick or D-pad changes the race, logo or car in menus (WIP, only partially implemented in this release).

### Other bits and pieces

- On modern (fast) systems, pressing enter in the menus could skip forward two or more screens, and pressing Esc could skip back two or more. This has now been fixed with a patched keyboard handling routine.
- Game language could be changed from the original Windows version's windowed mode. This can now be selected under "Advanced" in Big Red Launcher and is saved per profile.
- The colour animation of four levels (CHOPPER3, CITY4, MOON2, and RIG4) wasn't implemented in the 1996 Windows version, for some reason. Perhaps for performance, or perhaps it was an oversight. This palette cycling trick from the DOS version enables "flowing" water in the Italy (CITY4) track, for example, and really brings these tracks to life. It can now be fixed in these 4 tracks in the Windows version.
- The main menu now correctly reports the game version after patching (v2.25) as well as indicating that it has the additional modern patches applied.


## DOS patch 0.1.0 (Supports DOS 6+ and DOSBox)

`BRRPATCH.EXE` patches the DOS version's `RACING.EXE` in place, backing up the original as `RACING.ORI`. Running `BRRPATCH /U` puts it back. A Python version for Linux and macOS is in the zip as well, just in case you'd like to patch outside of a DOS environment or tweak what it does.

- **No more camera stutter:** Higher fidelity camera angles, as above.
- **Smooth movement on fast machines** (a fast Pentium, or DOSBox with plenty of cycles): the vehicles and the follow camera drawn between physics steps, 70 FPS instead of 35, track flyover and replays included.
- **Tear-free frames** on fast machines, frames are shown at the screen's vertical retrace.
- **A shorter catch-up** after a hiccup: at most a tenth of a second of game time in one frame, instead of half a second.
- Two camera calculations that depended on the frame rate have now been decoupled.
- **Joystick menu navigation** (partial): Button 1 (A) to proceed, Button 2 (B) to go back, left and right on the stick or D-pad changes the race, logo or car in menus (WIP, only partially implemented in this release).
- On a 486 machine, the game should run at the same speed as the original.
- **The physics is exactly as it was in 1996 - by design**.

## What you need

Both fixes only support the v2.25 patched version of the game, the last (and only) official update. The easiest way to get it is by downloading the two zips from [myabandonware.com](https://www.myabandonware.com/game/big-red-racing-2n8). You'll need the "Disc Image" rather than the "RIP Version" as the disc image includes the original CD audio tracks:

- `Big-Red-Racing_DOS_EN_Disc-Image.zip` (the original CD)
- `Big-Red-Racing_Patch_DOS_EN_Patch-v2-25.zip` (the official v2.25 patch)

Both fixes check the game's files and refuse any other version, changing nothing:

| File | Size (bytes) | SHA-256 |
|---|---|---|
| `WINRACE.EXE` (Windows) | 534,016 | `c0274f1248d552f09310a489cc34bc04a7e03f9485f40d24549b3a729752ecaa` |
| `RACING.EXE` (DOS) | 629,472 | `5186af0605ea87dfb8aee3463ff24acc259118fab611b07ec6e56b9f46c52636` |

## Installing

- **Windows 10 and 11:** unzip `big-red-launcher-0.1.0.zip` into a folder of its own, add the two zips, run `Install Game Files.bat`, then start `Big Red Racing.exe`.
- **Steam Deck and Linux:** set up the folder on a Windows PC and copy it across (or install the game yourself into its `Game` folder), then add `Big Red Racing.exe` to Steam as a non-Steam game and choose a Proton version. I intend to automate this process in future releases.
- **DOS:** copy `BRRPATCH.EXE` next to `RACING.EXE` and run `BRRPATCH`.

The full details are in each zip's `README.txt` / `README.TXT`, and the launcher's `Big Red Launcher Help.txt`.

## Known issues

- Windows' SmartScreen may warn about `Big Red Racing.exe` and `Install Game Files.bat` the first time, as they aren't signed: choose "More info", then "Run anyway".
- `Install Game Files.bat` needs Windows PowerShell, so it doesn't run under Linux or Proton.
- Split-screen races aren't drawn by the Modern renderer yet: they keep the game's own 640×480 picture and dashboard.
- The game draws in software: very tall resolutions and the furthest draw distances cost frame rate on slower PCs (lower "Max limit" or the draw distance if so).
- Some objects can still flicker in front of things behind them now and then, an engine limitation only partly fixed so far.
- It's not possible to fully navigate the menus using joystick input yet, making the game fiddly to use on Steam Deck and for couch joypad gaming. Keep a keyboard and mouse handy for now.

## Thanks

- Paul and Peter Ranson - and everyone else at Big Red Software!
- cnc-ddraw by FunkyFr3sh; dr_flac by David Reid; stb_vorbis by Sean Barrett.
- FLAC (Xiph.Org) and FFmpeg (builds by gyan.dev), which the installer can download to convert the music.

Big Red Launcher and the DOS patch are free software under the GNU General Public License, version 3 or later. *Big Red Racing* itself is not included, and remains the property of its rights holders.