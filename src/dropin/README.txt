Big Red Launcher by ctrl-alt-rees
=================================
ctrl-alt-rees.com

A launcher and a fix (PATCH.DLL) for the Windows 95 version of Big Red Racing
(1996), patched to v2.25. It makes the original game run on modern
Windows and on Linux/Steam Deck through Proton, and makes it run smoothly:

- No more "privileged instruction" crash: the game's direct reads of the 1996
  VGA card are replaced.
- A steady clock at 70 ticks a second, as the DOS version had: real time (the
  1996 Windows version ran 2% fast, at 71.43, also available).
- Smooth movement at any refresh rate: the vehicles and the follow camera are
  drawn between the game's physics steps, which happen 35 times a second.
- Two camera calculations that depended on the frame rate are fixed.
- No more camera stutter: the 3D engine could only tilt and turn the view in
  steps of 0.35 degrees, so the picture jolted up and down as the camera moved,
  and the camera's lean flicked with every step. The race is now drawn with
  finer angles and a steady lean (BRRFIX.INI, [camera]: fine_angles, roll_fix;
  set them to 0 for the original look).
- One key press, one choice in the menus: on a fast PC, pressing Return could
  also choose on the next screen, and Escape could go back two screens (or
  even quit), because the next screen appeared before the game had noticed
  the key let go (BRRFIX.INI, [fixes] menu_keys).
- The physics is left exactly as it was: the same game, bit for bit.
- The game's sharper 640x480 mode by default (it was only reachable from its
  window's menu or keypad +), and its language chosen in the launcher.
- The window's Full screen switch (and Tab), which broke the window, removed.
- The colour animation of four levels (CHOPPER3, CITY4, MOON2, RIG4), lost in
  the Windows version, restored as in DOS.
- The main menu says V2.25, the official update you installed (the game
  itself still said V2.24), with Big Red Launcher's version, and its credits
  line at the top includes "Camera / Modern Windows fix by ctrl-alt-rees".

It contains no part of the game, and your game's own files are left as they
are: the launcher runs its own copy of WINRACE.EXE, WINRACEP.EXE, which uses
PATCH.DLL in place of the game's TIMER.DLL (see Install). PATCH.DLL changes
the game's code only in memory while it runs, and only after checking that the
game is the exact v2.25 file and that every place it changes is as expected.
If anything doesn't match, it changes nothing, writes why to BRRFIX.LOG, and
works like the game's own TIMER.DLL.

You need
--------
Your own copy of Big Red Racing, patched to v2.25. The easy way, on Windows 10
and 11: the game's two zips from myabandonware.com (search there for Big Red
Racing), the disc image and the official v2.25 patch:
  Big-Red-Racing_DOS_EN_Disc-Image.zip
  Big-Red-Racing_Patch_DOS_EN_Patch-v2-25.zip
and "Install Game Files.bat" does the rest (see Install). Or a copy you have
installed and updated with the official patch yourself. Check: WINRACE.EXE
should be 534,016 bytes, SHA-256
@WINRACE_SHA256@

What's in this zip
------------------
- Big Red Racing.exe  Big Red Launcher: settings, checks, then starts the game
- Big Red Launcher Help.txt  what every setting in the launcher does
- PATCH.DLL           the fix itself (the game uses it instead of its own
                      TIMER.DLL)
- DPLAY.DLL           stands in for DirectX 3's DirectPlay, which Windows 10
                      and 11 lack (the game won't start without it); it passes
                      network play on to a real DirectPlay if there is one
- BRRFIX.INI          its settings (each one explained in the file)
- CNCDD.DLL, Shaders  cnc-ddraw, the open-source DirectDraw (MIT licence), for
                      modern Windows, Wine and Proton (its ddraw.dll, renamed
                      so that no other DirectDraw can take its place)
- ddraw.ini           our cnc-ddraw settings, with scaling presets
- Install Game Files.bat  makes the Game and Music folders from the game's
                      two zips (Windows 10 and 11; see Install)
- LICENSE.txt         Big Red Launcher's licence: the GNU GPL, version 3
- THIRD-PARTY.txt     the open-source parts and their licences

Install (Windows 95 to 11)
--------------------------
Big Red Launcher goes in a folder of its own, with your game in a folder
called Game inside it and the CD music in a folder called Music:

  Big Red Racing\          any name, anywhere you can write to
    Big Red Racing.exe     everything from this zip
    BRRFIX.INI ...
    WINRACEP.EXE           made by the launcher the first time
    Game\                  your game: WINRACE.EXE and the rest
    Music\                 the CD tracks (see Music)

1. Make a folder, for example C:\Games\Big Red Racing, and copy everything
   from this zip into it.
   On Windows 95, 98 and ME you can leave out CNCDD.DLL, ddraw.ini, Shaders and
   DPLAY.DLL: their own DirectX draws the game properly and has DirectPlay.
2. The game, the easy way (Windows 10 and 11): put the game's two zips (see
   You need) in the same folder and double-click "Install Game Files.bat".
   It checks the zips, installs the game into a folder called Game and patches
   it to v2.25, checking every file against the known game; converts the CD
   music into a folder called Music if you want it (FLAC, Ogg Vorbis or WAV;
   for FLAC or Ogg it downloads the converter, the official FLAC tool or
   FFmpeg, into a folder called Tools and checks it); and offers a desktop
   shortcut, and to delete the zips and the converter afterwards. Run it again
   at any time to check the Game folder: it puts back any of the game's files
   that are missing or changed, and leaves everything else alone. Windows may
   warn that it came from the internet ("More info", then "Run anyway"): it's
   a plain batch file, which you can read in Notepad.
   Or, with a copy you installed yourself: make a folder called Game inside it,
   and copy your installed game into it: everything in the game's folder (for
   example C:\BIGRED). Or leave the game where it is, and set [launcher] game=
   in BRRFIX.INI to its folder (game=C:\BIGRED).
3. The music goes in a folder called Music, next to Big Red Racing.exe:
   "Install Game Files.bat" puts it there, or see Music.
4. Start Big Red Racing.exe (make a shortcut to it if you like).
   It makes WINRACEP.EXE ("P" for patched) beside itself: a copy of the game's
   WINRACE.EXE with small changes. Its file headers are corrected, because
   the 1996 linker wrote them carelessly and Windows 10 and 11 refuse to start
   the original; the copy works on every version of Windows. It uses our
   PATCH.DLL where the game used its own TIMER.DLL, and, if CNCDD.DLL is
   there, cnc-ddraw where it used Windows' DirectDraw. The launcher starts it
   in the Game folder. Nothing in the Game folder is changed.
To remove it: delete the folder (the Game folder in it is your copy of the
game, so keep that if you need it).

Coming from an earlier Big Red Launcher, which went into the game's folder?
Move the game's files into a folder called Game as above. In it, delete our
old TIMER.DLL and rename TIMER.ORI back to TIMER.DLL (and WINRACE.ORI, if it's
there, back to WINRACE.EXE), and delete the earlier launcher's other files.
Put everything from this zip in the top folder, and rename MUSIC to Music and
move it there too.

Steam Deck and Linux (Proton)
-----------------------------
1. Install as above (the game folder can be anywhere, for example in your home
   folder). "Install Game Files.bat" needs Windows PowerShell, which Linux and
   Proton don't have: run it on a Windows PC and copy the whole folder across,
   or install the game yourself.
2. In Steam: Games > Add a Non-Steam Game to My Library > Browse, choose
   Big Red Racing.exe. Steam shows it as "Big Red Racing".
3. Right-click it > Properties > Compatibility > "Force the use of a specific
   Steam Play compatibility tool", and pick a Proton version.
4. Play. No launch options are needed: the game loads cnc-ddraw by its own
   name, so Proton can't use its own DirectDraw instead. Start Big Red
   Racing.exe, not WINRACEP.EXE: the game only finds its files when the
   launcher starts it in the Game folder.
On the Steam Deck the game fills the 1280x800 screen exactly (4x).

The launcher
------------
Its settings window covers the display, smoothness, music, controller and
advanced settings, and writes them into BRRFIX.INI and ddraw.ini (keeping their
comments). It works with the keyboard alone: Tab, arrow keys, Enter (Play) and
Esc (Quit), so a controller mapped to keys by Steam Input can use it.
- "Skip this window next time" starts the game straight away from then on.
- To see the window again: hold Shift while starting, or start with -setup.
- Steam launch options: -play skips the window, -setup always shows it.
Before starting the game it checks the Game folder's WINRACE.EXE is v2.25,
makes or refreshes WINRACEP.EXE from it, and checks that PATCH.DLL and
cnc-ddraw are there and that the music folder has tracks. It stays running
until the game ends, so Steam counts your play time, and then its window
comes back (unless it was skipped, or started with -play).

The DOS version
---------------
The launcher can also fix the DOS version (RACING.EXE, v2.24 from the v2.25
update) for playing on an old PC or in DOSBox: press "Patch the DOS version".
It patches RACING.EXE in the Game folder (or asks where the DOS version is),
keeping the original as RACING.ORI; press it again to put the original back.
This does the same as BRRPATCH.EXE, the DOS patch download, for PCs that can't
run DOS programs.

Music
-----
The game's music was CD audio. Without the CD, put the tracks in a folder
called Music next to Big Red Racing.exe, one file per CD track, named by track
number:
track02.flac, Track02.ogg, 02.wav, "02 - Title.flac" and so on (FLAC, Ogg
Vorbis or 16-bit WAV). "Install Game Files.bat" makes them from the game's disc
image (see Install), or rip your own CD. The game's own CD volume option works.
With no tracks there, the game plays a real CD if there is one.

Sound effects
-------------
The game fades other vehicles' sounds with their distance from the camera, but
on Windows it gave DirectSound its volumes in the wrong units, so every sound
played within 1.1 dB of full volume. Big Red Launcher converts them properly
("Distant vehicles quieter", on by default; [sound] distance_volume), as the
DOS version plays them: far trucks are quieter, the game's effects volume
option works, and the three engines heard at once are the nearest trucks'.
The stereo works too: the game panned a truck only a little off centre almost
entirely into one speaker; it now moves smoothly between them ("Natural
stereo"; [sound] stereo_pan). And it has 32 sound channels instead of 8 (or 16,
or the game's 8: "Sound channels"), with every truck's engine heard instead of
3 ([sound] channels).

Display and scaling
-------------------
Two renderers ("Renderer" in the launcher, [display] renderer in BRRFIX.INI):

Modern (renderer=new, the default; Windows 2000 onwards, Wine and Proton, with
cnc-ddraw): each picture goes to cnc-ddraw at its own size, crisp and with no
black borders (the menus' 320x200, races' 640x480 or bigger), and cnc-ddraw
enlarges it with the launcher's Scaling aspect and Scaling choices (Square and
Sharp by default). PrtSc does nothing (the game's own screenshots are left to
the Original renderer; ours are to come). In full
colour (32 bits) the sky is a smooth gradient instead of the game's 16 flat
bands, and lit surfaces (buildings, trees, vehicles, tunnel walls) are shaded
smoothly instead of a palette shade at a time ([display] full_colour,
smooth_sky, smooth_shading). The race dashboard (map, place, nitro, laps,
damage, messages) is the game's 320x200 art, drawn by the game on layers of
its own and enlarged by whole numbers into the race picture's corners
([display] hud=lowres; hud=game gives the game's own, as before; "4:3 HUD
correction" gives its pixels the 1996 monitor's shape). With Max in full
screen on a wide screen, races are drawn at the screen's own shape ("Native
widescreen", [display] widescreen; the menus stay 4:3 in the middle, their
background pictures widened across the screen: "Widescreen menu backgrounds",
[display] widescreen_menus).
Windowed, it's a plain window drawn by cnc-ddraw: Alt+Enter switches to full
screen and back. Its options: scaling, window size, colour depth,
"Resolution: Max" (the default: races drawn with a part of the screen's
height, "Max limit": half by default, a third, a quarter, a fifth or all of
it, at its shape with native widescreen, then enlarged to fill it; the game
draws in software, so more rows cost frame rate), and the
"Race world": "Draw distance" (the in-game sliders, the engine's own maximum
of 25 squares, Far at 44 or Max out at 60, the default, with the scenery and
vehicles as far as the ground), "Textures" (how far the ground is textured),
"Object lighting" (flat everywhere by default, or smooth; the game switched
to flat beyond 8 squares) and "Object detail" (every model at its most detailed
by default, or its simpler versions picked by its size on screen, so fences don't
change shape close up). The 3D engine's maths is done in 64 bits at today's resolutions,
where its 32 bits overflowed ([display] precise_3d). "Engine fixes": steady
ground textures (no switch 4 squares out), objects ordered by their footprint
so they don't show through hills, and gaps between level pieces closed
([display] ground_perspective, sort_footprint, close_gaps).

Original (renderer=original; the only one on Windows 95, 98 and ME): the 1996
game's own drawing, as players saw it, with only the smooth-motion fixes and
cnc-ddraw's full-screen scaling presets (Square and Sharp by default).
Windowed, it's the game's own window with its
1996 menu bar (About, Modes, Language) at the game's own size. Keypad + and -
switch between 640x480 and 320x200 as in 1996; the draw distance is the
in-game sliders'.

Both: the game's switch between a window and full screen (Tab, and the Modes
menu's Full screen item) broke the window and is removed: choose the mode in
the launcher. The colour cycling of four levels (CHOPPER3, CITY4 Italy, MOON2,
RIG4), which the DOS version has and the Windows version lost, runs again
("Colour animation", [display] colour_animation).

Controllers
-----------
Xbox-style pads (XInput, including the Steam Deck's controls) work as the
game's two joysticks: the first pad you use is joystick 1, the next one you
use joystick 2 (two players). Left stick steers, right trigger accelerates,
left trigger brakes and reverses, A and B are the joystick buttons. In a
race, Start pauses (and carries on) and Back (View) ends it, as Pause and Esc
do. In the menus A chooses (the 1996 game's own joystick
support), B goes back, as Esc, and left and right (stick or D-pad) change the
race, logo or car ([controller] menu_navigation). In the game's controls menu, choose the joystick (JS1 or JS2). Deadzone, sensitivity, the response curve and the
layout (triggers or stick) are in the launcher and in BRRFIX.INI. The pad
buzzes when your vehicle takes damage and rumbles while its nitro flames show
(the launcher's Vibration settings: either can be off, and an intensity).
No-code alternative: Steam Input can map the pad to keyboard keys instead
(steering is then on or off, as with the keyboard).
Not on Windows 95/98/ME (they have no XInput): use a joystick as in 1996.

Settings: BRRFIX.INI
--------------------
Every fix can be switched off, and the tick rate, the frame limit, vsync and
the logging can be changed. The file explains each setting. If it's missing,
the defaults are used.

Troubleshooting
---------------
- Something wrong? Look in BRRFIX.LOG next to Big Red Racing.exe: it says what
  was checked and changed. Set a fix to 0 in BRRFIX.INI to rule it out.
- The picture is tiny in a corner of a black screen: cnc-ddraw isn't being
  used. Check CNCDD.DLL is next to Big Red Racing.exe, and start the game
  with Big Red Racing.exe.
- "This program requires 256 colour mode to run windowed": BRRFIX.INI has
  [display] mode=ask; set it to windowed or fullscreen.
- No music: check the Music folder's file names (the launcher counts the
  tracks it finds) and the game's CD volume option.
- The pad does nothing: choose the joystick in the game's controls menu, and
  check [controller] enabled=1.

About
-----
Made for the Big Red Racing preservation project. Version @VERSION@.

Big Red Launcher is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version. It is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.txt for the details. Big Red
Racing itself is not included, and not covered by this licence. The parts by
others keep their own licences (THIRD-PARTY.txt).
