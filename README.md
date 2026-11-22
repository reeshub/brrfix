# Big Red Racing fixes: Big Red Launcher and BRRPATCH

Hello, my name's Rees, and back in the 90s I was obsessed with a ridiculous little PC gaming called *Big Red Racing*.

In recent years I've tried to revisit it and relive my youth, but alas, it ran incredibly badly in DOSBox and not at all under modern Windows. Even Wine didn't like it.

Well, I'm a big boy now, so I decided to take matters into my own hands and fix this sorry state of affairs once and for all. I've been fiddling with it for a few weeks now, so I suppose it's time I put something out there.

## What this is

Two free patch utilities by [ctrl-alt-rees](https://ctrl-alt-rees.com) for *Big Red Racing*, the 1996 racer by Big Red Software, published by Domark:

- **Big Red Launcher** runs the game's Windows 95 version on Windows 10 and 11, and on Linux (including the Steam Deck) through Wine or Proton. It can apply various fixes as detailed below, and even offers an optional "Modern" rendering engine.
- **BRRPATCH** applies camera and joystick fixes to the game's DOS version, for an optimal experience on old PCs and in DOSBox.

*Big Red Racing* was created by Big Red Software of Leamington Spa and published by Domark. While the game is officially considered "abandonware", this project contains no copyrighted assets and makes no claim as to the copyright of the game, which remains with the original rights holders.

> **This project is under very active development.** Please report anything that doesn't work in [Issues](https://github.com/reeshub/brrfix/issues), with the `BRRFIX.LOG` file from next to `Big Red Racing.exe` (tick "Log" on the launcher's Advanced tab first).

## Download

From the [Releases](https://github.com/reeshub/brrfix/releases) page:

| Download | For |
|---|---|
| `big-red-launcher-<version>.zip` | The Windows version: Supports Windows 95 to 11, Linux, and the Steam Deck. |
| `brr-dos-patch-<version>.zip` | The DOS version: Supports MS-DOS 6 and later, Windows 9x's MS-DOS Prompt and DOSBox. |

**Important Note:** Neither release nor any of the included source code contains any of Big Red / Domark's copyrighted material.

## Obtaining a copy of the game

Both fixes only support the v2.25 patched version of the game, the last (and only) official update. The easiest way to get it is by downloading the two zips from [myabandonware.com](https://www.myabandonware.com/game/big-red-racing-2n8). You'll need the "Disc Image" rather than the "RIP Version" as the disc image includes the original CD audio tracks.

| File | What it is |
|---|---|
| `Big-Red-Racing_DOS_EN_Disc-Image.zip` | The original CD as a bin/cue disc image. |
| `Big-Red-Racing_Patch_DOS_EN_Patch-v2-25.zip` | The official v2.25 patch. |

## Big Red Launcher

### What it does

- **WINRACE.EXE now actually starts and runs on modern Windows and Wine / Proton:** As a bare minimum, Big Red Launcher patches the original executable's broken file headers, bypasses a "privileged instruction" crash caused by a hardware call no longer permitted in modern Windows, bypasses the 256 colour mode check for windowed mode, and (on modern Windows) bundles cnc-ddraw, a compatibility wrapper for DirectPlay, which has been hand-tweaked by the author for this game.
- **Fix the jittery camera:** For years, it was believed that the game's stuttery performance on faster systems was a result of the physics engine being tied to the system's clock. It turns out that this isn't strictly the case (you can read more about this in the included documentation, if you like). Big Red Launcher can (optionally, enabled by default) patch the game to hugely increase the in-game camera's positioning fidelity and update speed. The physics engine is independent of this, so the game plays exactly the same (quirks and all).
- **"Original" vs. "Modern" renderer:** In addition to the above, there's a new "Modern" renderer option. OK, so it's still the game's original 1996 DirectPlay renderer wrapped in cnc-ddraw, but it does add a whole host of tweaks, like native widescreen support, full colour with improved gradients and shading, hugely improved draw distance, level geometry fixes (WIP), and a lot more. Again, the golden rule of development was to retain the original feel of the game even with this major overhaul, and I feel that I've succeeded in this.
- **CD music:** Supports ripped tracks (FLAC, Ogg Vorbis or WAV). This is (optionally) supported in both the Original and Modern renderers, with the help of the included open source [dr_flac](https://github.com/mackron/dr_libs) and [stb_vorbis](https://github.com/nothings/stb) libraries. Or you can stick your original CD in the drive and listen to that - or indeed any audio CD of your choice, just like back in the day. Scripts are included to rip and convert these tracks from the myabandonware.com disc image.
- **Other in-game audio improvements:** The DOS build of the game used the Miles Audio engine, which supported panning (ie, competitor vehicle audio moves left to right depending on their position relative to the player), and varying volume levels depending on their distance. It turns out that both were broken in the 1996 Windows DirectSound implementation - in fact, audio was pretty broken overall in Windows. There are now (optional) fixes for these issues and more. The game's 8 sound channels can also optionally be expanded to 16 or 32 for a more immersive experience.
- **Modern controllers** (XInput), with remapped triggers for accelerate / brake, vibration, and menu navigation support.
- **A comprehensive settings window**, with "Period" and "Modern" defaults, and detailed help for every single setting.

Did I mention that all tweaks can be toggled on or off depending on how you'd like to experience the game? Big Red Launcher is also designed to support all versions of Windows from 95 onwards, and supports resolutions as low as 640x480. It also works in Wine and Proton.

In all cases, the original game files are left as they are: the launcher creates and runs its own patched copies.

### Install on Windows 10 or 11

1. Unzip `big-red-launcher-<version>.zip` into a folder of its own, for example `C:\Games\Big Red Racing`.
2. Put the game's two zips in the same folder and run **`Install Game Files.bat`**. It installs and patches the game into a `Game` folder (verifying every file), and (optionally) rips the CD music into a `Music` folder in the format of your choice.
3. Start **`Big Red Racing.exe`**.

**Windows Defender SmartScreen will almost definitely warn that it came from the internet: choose "More info", then "Run anyway". You'll only have to do this once.**

If you already have the game installed and patched to v2.25, copy the whole shebang into a folder called `Game` alongside Big Red Launcher and it should be able to work with it.

### Steam Deck and Linux

`Install Game Files.bat` needs Windows PowerShell: run it on a Windows PC and copy the whole folder across, or install the game yourself. Then, in Steam: *Games > Add a Non-Steam Game to My Library*, choose `Big Red Racing.exe`, and under *Properties > Compatibility* pick a Proton version. No launch options are needed.

Note that there are future plans to automate this process, as well as creation of a Lutris installer.

### Windows 95, 98 and ME

Big Red Launcher is supported, but limited to the Original renderer and a handful of tweaks: see `README.txt` in the zip.

Note that some Big Red Launcher tweaks rely on modern Windows - or would run extremely poorly on period hardware - so they're hidden as appropriate when running under Windows 9x. You can still edit the INI file to try these out if you like breaking things for fun (trust me, I can relate).

## BRRPATCH.EXE (DOS 6+, DOSBox on modern systems)

This patcher fixes the camera's infamous stutter by applying the same high fidelity camera fix as the Windows version, as well as increasing the draw rate (70 FPS instead of 35), and shortens the camera catch-up after a hiccup. It also adds joystick and keyboard menu navigation. A 486 runs exactly as fast as the original release, and crucially the in-game physics is unchanged, preserving the original feel of the game.

Copy `BRRPATCH.EXE` into the game's folder and run it there: it asks which fixes you'd like, patches `RACING.EXE` (keeping the original as `RACING.ORI`) and starts the game. Run it again to change your choices; `BRRPATCH /U` puts the original back. On modern Windows, Big Red Launcher's "Patch the DOS version" button does the same, and the zip also includes a Python version for Linux and macOS DOSBox users.

## Building from source

See [BUILDING.md](BUILDING.md).

## AI Disclosure

If you're not familiar with my work (and why would you be?), I'm a professional software developer with over 20 years' experience. That said, I have neither the time nor inclination to learn the ins and outs of Watcom's particular 1990s flavour of the C programming language, and I just wanted to get on and play one of my favourite childhood games. As this AI thing has completely turned my industry on its head in recent years, I also wanted to learn about agentic coding in the process.

To that end, I used Claude Opus 5.5 running under VS Code CoPilot extensively in this project.

That said, this is very much a human-directed project, with all code changes thoroughly vetted and tested at every step. If anything comes up that I don't understand, I do my own research. I've learned an awful lot about DOS and Windows 9x game development as a result and strive to understand any and all changes made to the game's code - to the best of my ability. I feel that this is reflected in the quality of the end product.

I appreciate that this new technology is controversial, so I wanted to be 100% transparent about this. I'll be covering the entire process in detail on [my YouTube channel](https://www.youtube.com/ctrlaltrees) in due course.

## Licence

Big Red Launcher, BRRPATCH and the rest of this repository's own code are free software: you can redistribute them and/or modify them under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. They are distributed in the hope that they will be useful, but WITHOUT ANY WARRANTY; see [LICENSE](LICENSE) for the details.

*Big Red Racing* itself is not included, and remains the property of its rights holders. The parts by others keep their own licences: see [THIRD-PARTY.txt](THIRD-PARTY.txt).

## Thanks

- Paul and Peter Ranson - and everyone else at Big Red Software!
- [cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw) by FunkyFr3sh, which draws the game on modern Windows, Wine and Proton.
- [dr_flac](https://github.com/mackron/dr_libs) by David Reid and [stb_vorbis](https://github.com/nothings/stb) by Sean Barrett, which play the music.
- [FLAC](https://xiph.org/flac/) (Xiph.Org) and [FFmpeg](https://ffmpeg.org) (builds by [gyan.dev](https://www.gyan.dev/ffmpeg/builds/)), which `Install Game Files.bat` can download to convert the music.
- The menu navigation click, "UI Click Deep": Sound Effect by <a href="https://pixabay.com/users/soundshelfstudio-46480698/?utm_source=link-attribution&utm_medium=referral&utm_campaign=music&utm_content=512213">SoundShelfStudio</a> from <a href="https://pixabay.com/sound-effects//?utm_source=link-attribution&utm_medium=referral&utm_campaign=music&utm_content=512213">Pixabay</a>.
