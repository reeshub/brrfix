# Big Red Launcher @VERSION@ by ctrl-alt-rees

ctrl-alt-rees.com

Big Red Launcher fixes and starts the Windows version of Big Red Racing (1996),
and lets you choose its settings. This file explains every setting
in its window. Rest the mouse on a setting to see a one-line reminder.

Start the game with **Big Red Racing.exe**. To see the settings window again
after ticking "Skip this window next time", hold **Shift** while starting it,
or start it with **-setup**.

## Display

**Mode**

- **Full screen** (the default): the game fills the screen, without asking.
- **Windowed:** the game's own 1996 window, with its menu bar (About,
  Modes, Language), as on a Windows 95 desktop. Its Modes menu no longer
  offers Full screen, and Tab no longer switches: that switch broke the
  window. Choose the mode here instead.

**Resolution**

- **High (640x480)** (the default): the game's own sharper mode, which it
  only offered through its window's Modes menu or keypad +. It applies from
  the first screen. Races are drawn at 640x480; the menus are the same
  320x200 pictures, doubled.
- **Low (320x200):** as the game starts.

In the game, keypad + and - still switch between them.

**Window size** (in a window only)

How big the window is, in whole multiples of the game's picture (320x200 or
640x480). "Largest that fits" picks the biggest that fits on your screen.

**4:3 (non-square pixels)** (in a window at low resolution only; on by default)

The game's low-resolution picture is 320x200, but 1996 monitors showed it at
4:3, so everything looked a little taller. Ticked, the picture's height is
stretched by 1.2 to match, as the game was meant to be seen. Unticked, pixels
are square. At 640x480 the picture already has the 4:3 shape.

**Full screen** (in full screen only; not on Windows 95, 98 or ME)

How the picture fills the screen:

- **Integer (sharp):** the largest whole-number size that fits, with square
  pixels and black borders. 4x on the Steam Deck, which fills its screen.
  At high resolution (640x480) a screen needs to be at least 960 pixels tall
  for 2x; on smaller screens the picture is shown at 1x, so 4:3 sharp may
  suit them better.
- **4:3 sharp:** the 1996 monitor shape, as big as fits, kept sharp.
- **4:3 nearest:** the same shape with plain pixels; some rows and columns are
  one screen pixel wider than others.
- **Stretch:** fills the whole screen, whatever its shape.
- **Smooth:** slightly blurred, as cnc-ddraw does by default.

## Smoothness

**Interpolation (smooth motion)**

The game's physics moves the vehicles 35 times a second. With interpolation,
the vehicles and the camera are drawn in between those steps, so a modern PC
shows smooth motion at its full frame rate instead of 35 jerky steps a second.
The physics itself is exactly the same either way.

**Camera smoothing**

Experimental: softens the camera's movement further. The camera stutter is
already fixed without it, so leave it Off unless you want to experiment.

**Frame limit**

The most pictures drawn each second. "Display rate" matches your screen's
refresh rate, which is usually best. "Unlocked" draws as many as your PC can.
The game itself runs at the same speed whatever you choose.

**Vsync**

Waits for the screen to finish each refresh before showing the next picture,
so there are no horizontal tears when the camera turns. It can add a little
input lag.

## Game

**Language**

The game's language, as in its window's Language menu: English (the default),
USA, French, German, Italian, Spanish or Japanese. It applies from the first
screen, "Enter your name". The window's Language menu still works, but only
until the game is closed: next time, this setting is used again.

## Music

**CD music from ripped tracks**

The game's music was CD audio. Ticked, it's played from ripped tracks in the
music folder instead, with no CD needed. Put one file per CD track in the
folder, named by track number: track02.flac, Track02.ogg, 02.wav,
"02 - Title.flac" and so on (FLAC, Ogg Vorbis or 16-bit WAV). The window says
how many tracks it found.

**Volume**

The music's volume, on top of the game's own CD volume option.

**Folder and Browse**

Where the ripped tracks are: MUSIC (a folder next to the game) by default, or
any folder you choose with Browse.

## Controller

**Configure**

Opens Windows' own game controller settings, where you can set up and
calibrate a joystick or pad (the Windows version of the game has no
calibration screen of its own; the DOS version does). Not shown under
Proton, where Steam sets up controllers.

The settings below are not on Windows 95, 98 or ME, which have no support
for modern pads.

**Modern pad (XInput)**

An Xbox-style pad, including the Steam Deck's controls, works as the game's
joystick: the left stick steers, and A and B are the joystick's buttons. In the
game, choose the joystick in the controls menu.

**Deadzone**

How far the stick has to move before steering starts. Raise it if the vehicle
drifts with the stick at rest.

**Sensitivity**

How much stick movement gives full lock: at 100% you reach full lock at the
end of the stick's travel; higher values reach it sooner.

**Layout**

- **Triggers:** the right trigger accelerates, the left one brakes and
  reverses.
- **Stick:** push the stick up to accelerate and down to brake, as with the
  original joystick.

## Advanced

**Tick rate**

The game's clock "ticks" a fixed number of times a second, and everything in
it, the physics, the timers and the countdown, moves on by ticks.

- **70.0 (DOS original):** the DOS version's rate, which matches real seconds
  (the default).
- **71.43 (Windows original):** the rate the 1996 Windows version ran at,
  about 2% faster than real time.

**Catch-up**

The game runs its physics in ticks, and before each picture it runs as many
ticks as have passed on the clock since the last picture. Usually that's one
or two. But if the PC pauses for a moment (loading, another program, a hiccup
in Windows), many ticks pile up, and the game runs them all at once before it
draws again: on screen, everything jumps forward to where it should be by now.

Catch-up sets how many ticks it will run at once:

- **8 ticks** (the default): at most about a tenth of a second at once. After a
  long pause the game skips the rest of the missed time instead of jumping, so
  the picture stays steadier, and the race carries on a fraction of a second
  behind the clock.
- **35 (original):** up to half a second at once, as in 1996.

The physics is the same either way; only how a pause is made up changes. If in doubt, pick 8 on modern systems as it will make the game feel smoother.

**Log**

Writes BRRFIX.LOG next to the game: what was fixed and how the clock ran. It's
off by default; turn it on if you're reporting a problem, and send the file
with your report.

## The buttons

- **Skip this window next time:** starts the game straight away from now on.
  Hold Shift while starting, or start with -setup, to see this window again.
- **Patch the DOS version:** fixes the DOS version's RACING.EXE (the same game
  fixes, for DOS PCs and DOSBox), keeping the original as RACING.ORI. Press it
  again on a patched file to put the original back.
- **Help:** opens this file (or press F1).
- **Defaults:** puts every setting back to its default. Press Save or Play to
  keep them.
- **Play:** saves the settings and starts the game.
- **Save:** saves the settings without starting the game.
- **Quit:** closes the window without saving.

## Correcting WINRACE.EXE

Windows 10 and 11 refuse to start the 1996 WINRACE.EXE as it is: the linker
that made it in 1996 wrote its file headers carelessly, which Windows 95 didn't
mind. The first time you press Play, the launcher offers to correct them. Only
the headers change; the corrected file works on every version of
Windows, and the original is kept as WINRACE.ORI. To undo, delete WINRACE.EXE
and rename WINRACE.ORI to WINRACE.EXE.

## Starting from Steam

Add Big Red Racing.exe to Steam as a non-Steam game. Launch options:
**-play** skips the settings window, **-setup** always shows it. Any other
options are passed on to the game.

## Where the settings are kept

BRRFIX.INI (the fix's settings) and ddraw.ini (cnc-ddraw's, for full screen),
next to the game. Each setting is explained in the files, and some that the
window doesn't show can be changed there. The launcher changes only the values
it shows, and keeps everything else in the files as it was.
