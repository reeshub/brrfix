Big Red Launcher by ctrl-alt-rees
=================================
ctrl-alt-rees.com

A replacement TIMER.DLL for the Windows 95 version of Big Red Racing (1996),
patched to v2.25. It makes the original game run on modern Windows and on
Linux/Steam Deck through Proton, and makes it run smoothly:

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
- The main menu says V2.25, the official update you installed (the game
  itself still said V2.24), with Big Red Launcher's version, and its credits
  line at the top includes "Camera / Modern Windows fix by ctrl-alt-rees".

It contains no part of the game. TIMER.DLL changes the game's code only in
memory while it runs, and only after checking that WINRACE.EXE is the exact
v2.25 file and that every place it changes is as expected. If anything doesn't
match, it changes nothing, writes why to BRRFIX.LOG, and works like the
original TIMER.DLL. (The launcher also corrects WINRACE.EXE's file headers on
disk, keeping the original: see Install.)

You need
--------
Your own copy of Big Red Racing, installed and updated to v2.25 with the
official patch. Check: WINRACE.EXE should be 534,016 bytes, SHA-256
@WINRACE_SHA256@

What's in this zip
------------------
- Big Red Racing.exe  Big Red Launcher: settings, checks, then starts the game
- Big Red Launcher Help.txt  what every setting in the launcher does
- TIMER.DLL           the fix itself (replaces the game's own TIMER.DLL)
- DPLAY.DLL           stands in for DirectX 3's DirectPlay, which Windows 10
                      and 11 lack (the game won't start without it); it passes
                      network play on to a real DirectPlay if there is one
- BRRFIX.INI          its settings (each one explained in the file)
- ddraw.dll, Shaders  cnc-ddraw, the open-source DirectDraw (MIT licence), for
                      modern Windows, Wine and Proton
- ddraw.ini           our cnc-ddraw settings, with scaling presets
- THIRD-PARTY.txt     the open-source parts and their licences

Install (Windows 95 to 11)
--------------------------
1. Back up: in the game folder (for example C:\BIGRED), rename the game's own
   TIMER.DLL to TIMER.ORI.
2. Copy everything from this zip into the game folder.
   On Windows 95, 98 and ME you can leave out ddraw.dll, ddraw.ini, Shaders and
   DPLAY.DLL: their own DirectX draws the game properly and has DirectPlay.
3. Start Big Red Racing.exe (make a shortcut to it if you like).
   The first time, it offers to correct WINRACE.EXE: the 1996 linker wrote its
   file headers carelessly, and Windows 10 and 11 refuse to start it. Only the
   headers change, the corrected file works on every version of Windows, and
   the original is kept as WINRACE.ORI.
To undo: delete the files from this zip and rename TIMER.ORI back to TIMER.DLL
(and, if it's there, WINRACE.ORI back to WINRACE.EXE).

Steam Deck and Linux (Proton)
-----------------------------
1. Install as above (the game folder can be anywhere, for example in your home
   folder).
2. In Steam: Games > Add a Non-Steam Game to My Library > Browse, choose
   Big Red Racing.exe. Steam shows it as "Big Red Racing".
3. Right-click it > Properties > Compatibility > "Force the use of a specific
   Steam Play compatibility tool", and pick a Proton version.
4. Play. No launch options are needed: the launcher tells Proton to use
   cnc-ddraw. (Starting WINRACE.EXE directly instead? Then add the launch
   options WINEDLLOVERRIDES="ddraw=n,b" %command%.)
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
Before starting the game it checks WINRACE.EXE is v2.25, that our TIMER.DLL and
cnc-ddraw are there, and that the music folder has tracks. It stays running
until the game ends, so Steam counts your play time.

The DOS version
---------------
The launcher can also fix the DOS version (RACING.EXE, v2.24 from the v2.25
update) for playing on an old PC or in DOSBox: press "Patch the DOS version".
It patches RACING.EXE in the game folder (or asks where the DOS version is),
keeping the original as RACING.ORI; press it again to put the original back.
This does the same as BRRPATCH.EXE, the DOS patch download, for PCs that can't
run DOS programs.

Music
-----
The game's music was CD audio. Without the CD, put the tracks in a folder
called MUSIC next to the game, one file per CD track, named by track number:
track02.flac, Track02.ogg, 02.wav, "02 - Title.flac" and so on (FLAC, Ogg
Vorbis or 16-bit WAV). Either rip your own CD, or, with the project's tools,
run "python3 brr.py prepare" with your own copy of the game's disc image: it
writes the tracks to work/disc/audio/. The game's own CD volume option works.
With no tracks there, the game plays a real CD if there is one.

Display and scaling
-------------------
By default the game fills the screen, at your desktop's own resolution, with
sharp whole-number scaling. The new renderer ([display] renderer=new, the
default; "Renderer" in the launcher) makes the picture once, at the screen's
own resolution (full screen) or the window's size (windowed), enlarging the
game's picture by whole numbers on black, so nothing is scaled twice. Windows
95, 98 and ME, and full screen without cnc-ddraw, keep the game's own
drawing (renderer=original), which cnc-ddraw scales with its presets.
Windowed: pick it in the launcher (or [display]
mode=windowed in BRRFIX.INI, and windowed=true in ddraw.ini): the game's own
window with its 1996 menu bar (About, Modes, Language), as big as fits on your
screen, with exact colours. For full screen with the original renderer,
ddraw.ini has presets:
integer (sharp, square pixels; the default), 4:3 sharp, 4:3 nearest, stretch
and smooth.

Experimental: "Draw distance" in the launcher ([display] max_draw_distance)
draws races and replays further: 25 squares (the most the 1996 engine holds
on its own; its slider stops at 22) with textures all the way out, or 44
(double) with extra passes for the ground beyond. The in-game sliders are
ignored unless it's set to the game's own.

Experimental: "In-game render resolution" in the launcher ([display]
render_resolution=max; new renderer only) draws races and the attract-mode
demo with more pixels: up to 1008x756, the most the game's 3D engine can draw,
at the 4:3 size that fills the most of the screen when enlarged by a whole
number (1008x756 on 1280x800 or 1600x900, 720x540 doubled on 1920x1080). The
dashboard is the game's own, enlarged by the game to match.

Controllers
-----------
An Xbox-style pad (XInput, including the Steam Deck's controls) works as the
game's joystick: left stick steers, right trigger accelerates, left trigger
brakes and reverses, A and B are the joystick buttons. In the game's controls
menu, choose the joystick. Deadzone, sensitivity, the response curve and the
layout (triggers or stick) are in the launcher and in BRRFIX.INI.
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
- Something wrong? Look in BRRFIX.LOG in the game folder: it says what was
  checked and changed. Set a fix to 0 in BRRFIX.INI to rule it out.
- The picture is tiny in a corner of a black screen: cnc-ddraw isn't being
  used. Start the game with Big Red Racing.exe, or set the DLL override
  ddraw=n,b (see above).
- "This program requires 256 colour mode to run windowed": BRRFIX.INI has
  [display] mode=ask; set it to windowed or fullscreen.
- No music: check the MUSIC folder's file names (the launcher counts the
  tracks it finds) and the game's CD volume option.
- The pad does nothing: choose the joystick in the game's controls menu, and
  check [controller] enabled=1.

About
-----
Made for the Big Red Racing preservation project, with the blessing of Paul
and Peter Ranson of Big Red Software. Version @VERSION@.
