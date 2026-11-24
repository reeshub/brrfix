# Big Red Racing fixes 0.1.2 - "The Controller Release"

You can now play the whole game with a controller - for the first time since 1996 - without ever needing to touch a keyboard or mouse. Great news for Proton gamers! Menu keyboard navigation is also massively improved, and this improved keyboard and joystick navigation applies in both the Windows and DOS versions of the game as a configurable option.

This release also fixes a crash in the attract demo, makes the pause behave properly (with a PAUSED sign and an "End Race?" confirmation on Windows), and has a few smaller improvements.

**This is still a testing release:** please report anything that doesn't work in [Issues](https://github.com/reeshub/brrfix/issues).

| Download | For | Version |
|---|---|---|
| `big-red-launcher-0.1.2.zip` | The Windows version, on Windows 95 to 11, Linux and the Steam Deck (Proton) | Big Red Launcher 0.1.2 |
| `brr-dos-patch-0.1.1.zip` | The DOS version, on MS-DOS 6 and later, Windows 9x's MS-DOS Prompt and DOSBox | DOS patch 0.1.1 |

Neither download contains the game itself: both work on your own copy, patched to v2.25 with the official update.

## Big Red Launcher 0.1.2

### Crash fix

- **The attract demo could crash** with the Modern renderer in 0.1.0 and 0.1.1 (and, probably more rarely, a race with team 4's driver on screen). Each driver's head has an extra-detailed close-up version, which the game only draws when the camera is right next to it. In one of them, the mouth was made as a textured polygon with no texture: something undiscovered until today because that version was never drawn in a race. Our new renderer with object detail "Highest" (the default) drew the close-up heads at any distance, so the bad polygon was drawn, resulting in a hard crash. The drivers' heads now keep the game's own choice of detail. I plan to fix the model itself in a future release.

### Menus with a controller or the keyboard

- **Every menu can be navigated** with a pad, a joystick or the keyboard's arrow keys. Up and down step through the items in reading order, left and right move a slider or change the race, logo or car, A (or Return) chooses and B (or Esc) goes back. The mouse pointer is automatically hidden, and moving the mouse hands the menus back to it.
- **The name screen** starts with the last name you entered: A takes it, and A and B answer Y and N to "Create new file?". Also, as in 1996, you can just skip it entirely and enter a blank name. I had no idea this was even a thing until now.
- **Menu animations:** Lots of new options, so you can actually see what you're selecting when using the controller or keyboard. I've tried to keep these within the original spirit of the game but this is a first stab and I'm open to suggestions. Options are "Inverted" (the default): only the selected item animates, so you can see at a glance where you are, or "Original-ish": everything animates except the selected item. These can be found under "Controller" in the launcher, and there's also a "Disable animated titles" options to keep the screens' headings still. Note that some single-purpose screens still animate despite this setting, which is by design. We can't strip *all* of the wackiness out after all. It's what the original designers would have wanted ;)
- Using a joystick in the menus automatically sets player 1's controls to it (you can override this on the Joysticks screen).

### Controllers

- **Pads without Xbox support** (that Windows reads as a joystick, such as an 8BitDo Micro in D-input mode) now work fully: their D-pad, and on 8BitDo pads L2 and R2 as the triggers, Start and Select.
- **Analogue sticks and/or D-pad steers in races** on every pad.
- **Automatic analogue stick detection:** the in-game menu now autodetects the joystick type.

### Pausing and ending a race (Modern renderer)

- **PAUSED** appears in the middle of the screen while a race is paused, in the menus' own animated lettering, with the race darkened behind it.
    - **No pausing before the lights turn green.** In 1996, pausing during the countdown didn't stop the camera's swoop onto the grid or the countdown's commentary. This is now fixed.
    - **The sound effects and commentary now pause with the race** and carry on where they left off. In 1996, whatever was playing (engines, skids) carried on looping during the pause, which was annoying. The glorious in-game music plays on, of course.
- **"End Race?":** Esc or Back no longer ends a race straight away. It pauses the race and asks, with Yes and No: left and right choose, A or Return does it, B or Esc goes straight back to the race (Modern renderer).

### Other changes

- **The title screen at the start now stays for 5 seconds**. Originally this was a loading screen, but on modern PCs we didn't even get time to admire it. Of course Esc, Space, Enter, a click or any controller button skips it. In widescreen mode with the widescreen menus enabled, it's a widescreen version of the picture (Modern renderer).
- **The track map on the dashboard** is back to the size of the rest of the dashboard (it was twice that). Poor design decision on my part. Sorry about that.
- **The CD music volume** is back on the game's Sound options screen, as in the DOS version (the 1996 Windows version left it out).
- **The Original renderer always uses the game's 8 sound channels**, so the setting is only shown with the Modern renderer. That includes Windows 95, 98 and ME, which only have the Original renderer. It turns out Win9x can't handle that many channels, who knew?
- **The mouse works in the menus under Wine and Proton** from the start (before, cnc-ddraw ignored it until it had first captured the pointer).
- Help file and `README.txt` updated.

## DOS patch 0.1.1

- **Every menu can be navigated** with a joystick or the arrow keys, as on Windows: up and down step through the items, left and right move a slider or change the race, logo or car, button 1 (or Return) chooses and button 2 (or Esc) goes back, with a click as the selection moves. The name screen starts with the last name entered, using the joystick in the menus sets player 1 to it, and any input on the main menu holds back the attract demo.
- **Menu animations:** Inverted (the default) or Original-ish, as on Windows.
- **BRRPATCH asks about each fix** with a sentence on what it does (Y or N; Return for yes), then about the menu animations (I or O), and then starts the game. Run it again to change your answers. `BRRPATCH /Y` patches with every fix without asking (and doesn't start the game), `BRRPATCH /N` asks but doesn't start the game, and `BRRPATCH /U` still puts the original back.
- **Updating from 0.1.0:** the new BRRPATCH recognises a `RACING.EXE` patched by 0.1.0 and rebuilds it from the saved original, `RACING.ORI`.

Everything else is as in [0.1.0](https://github.com/reeshub/brrfix/releases/tag/v0.1.0), whose notes list all of the fixes.

## Updating

- **Big Red Launcher:** unzip `big-red-launcher-0.1.2.zip` over your Big Red Launcher folder: your `Game` and `Music` folders stay as they are. Windows will ask whether to replace `BRRFIX.INI`, which holds your saved settings: replace it to start from 0.1.2's defaults, or keep yours to keep your settings (the new settings take their defaults either way).
- **DOS patch:** copy the new `BRRPATCH.EXE` over the old one and run `BRRPATCH`. Keep `RACING.ORI`: it's needed to update or undo the patch.

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

- **Windows 10 and 11:** unzip `big-red-launcher-0.1.2.zip` into a folder of its own, add the two zips, run `Install Game Files.bat`, then start `Big Red Racing.exe`.
- **Windows 95, 98 and ME:** set up the folder on a newer PC with `Install Game Files.bat` and copy it across, or copy your own installed game into its `Game` folder. You can leave out `CNCDD.DLL`, `ddraw.ini`, `Shaders` and `DPLAY.DLL`: those versions' own DirectX draws the game and has DirectPlay. Start the game with `Big Red Racing.exe`, not `WINRACEP.EXE`.
- **Steam Deck and Linux:** set up the folder on a Windows PC and copy it across (or install the game yourself into its `Game` folder), then add `Big Red Racing.exe` to Steam as a non-Steam game and choose a Proton version.
- **DOS:** copy `BRRPATCH.EXE` next to `RACING.EXE` and run `BRRPATCH`.

The full details are in each zip's `README.txt` / `README.TXT`, and the launcher's `Big Red Launcher Help.txt`.

## Known issues

- Windows' SmartScreen may warn about `Big Red Racing.exe` and `Install Game Files.bat` the first time, as they aren't signed: choose "More info", then "Run anyway".
- `Install Game Files.bat` needs Windows PowerShell 5.1 (Windows 10 and 11), so it doesn't run under Linux, Proton or Windows 95, 98 and ME.
- Split-screen races aren't drawn by the Modern renderer yet: they keep the game's own 640×480 picture and dashboard, without PAUSED or "End Race?".
- The game draws in software: very tall resolutions and the furthest draw distances cost frame rate on slower PCs (lower "Max limit" or the draw distance if so).
- Some objects can still flicker in front of things behind them now and then, an engine limitation only partly fixed so far.
- Pads without Xbox support other than 8BitDo's: the stick, D-pad and two buttons work, but their extra buttons (triggers, Start, Select) aren't mapped yet. If yours isn't, please tell us which pad it is in [Issues](https://github.com/reeshub/brrfix/issues) (with `BRRFIX.LOG`: Advanced tab, Log).

## Thanks

- Paul and Peter Ranson - and everyone else at Big Red Software!
- cnc-ddraw by FunkyFr3sh; dr_flac by David Reid; stb_vorbis by Sean Barrett.
- FLAC (Xiph.Org) and FFmpeg (builds by gyan.dev), which the installer can download to convert the music.
- The menu navigation click, "UI Click Deep": sound effect by [SoundShelfStudio](https://pixabay.com/users/soundshelfstudio-46480698/) from [Pixabay](https://pixabay.com/sound-effects/).

Big Red Launcher and the DOS patch are free software under the GNU General Public License, version 3 or later. *Big Red Racing* itself is not included, and remains the property of its rights holders.
