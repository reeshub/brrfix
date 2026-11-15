# Big Red Launcher @VERSION@ by ctrl-alt-rees

ctrl-alt-rees.com

Big Red Launcher fixes and starts the Windows version of Big Red Racing (1996),
and lets you choose its settings. This file explains every setting
in its window. Rest the mouse on a setting to see a one-line reminder.

Start the game with **Big Red Racing.exe**. To see the settings window again
after ticking "Skip this window next time", hold **Shift** while starting it,
or start it with **-setup**.

The settings are on five tabs: General, Video, Audio, Controller and Advanced.
Click a tab, or use the arrow keys while the tabs are selected (as they are
when the window opens). This file follows the same order.

## General

Who made Big Red Launcher, what it does, and two sets of settings to start
from:

- **Period defaults:** for a 1990s PC with Windows 95, 98 or ME, where the
  game's 640x480 3D is heavy work: full screen at the game's low resolution
  (320x200), drawn by the original renderer, with interpolation on and the
  frame limit at the display rate.
- **Modern defaults:** for today's Windows, Linux and Steam Deck: full screen
  at high resolution (640x480), drawn sharp by the new renderer, with modern
  pads on.

Each button sets every setting on every tab. Press Save or Play to keep them.
The Defaults button at the bottom of the window chooses the set for your PC:
Period defaults on Windows 95, 98 and ME, Modern defaults on anything newer.

## Video

### Display

**Renderer** (not on Windows 95, 98 or ME, which always use the original)

- **New (sharp)** (the default): the picture is made once, at your screen's
  own resolution (full screen) or your window's size (windowed), as a whole-
  number enlargement of the game's picture centred on black, and shown with no
  scaling after that. Nothing is scaled twice, so it stays sharp on any
  screen. The 320x200 menus are enlarged by the whole numbers across and
  down that come nearest to the 4:3 shape of a 1996 monitor (see "4:3"
  below); 640x480 races are enlarged by the same whole number both ways. In
  full screen it needs cnc-ddraw (CNCDD.DLL, next to Big Red Racing.exe), as
  the game does on modern Windows; without it, the original is used. It works
  in full colour, so the sky is a smooth gradient instead of the game's
  16 flat bands, and lit surfaces are shaded smoothly instead of a palette
  shade at a time (BRRFIX.INI's [display] full_colour, smooth_sky and
  smooth_shading turn these off).
- **Original:** the game's own drawing. In full screen cnc-ddraw enlarges it
  (see "Full screen" below); the menus are 320x200 pictures doubled into the
  middle of a 640x480 picture, with black bands above and below.

**Mode**

- **Full screen** (the default): the game fills the screen, without asking.
- **Windowed:** the game's own 1996 window, with its menu bar (About,
  Modes, Language), as on a Windows 95 desktop. Its Modes menu no longer
  offers Full screen, and Tab no longer switches: that switch broke the
  window. Choose the mode here instead.

**Resolution**

- **High (640x480)** (the default): the game's own sharper mode, which it
  only offered through its window's Modes menu or keypad +. It applies from
  the first screen. Races are drawn at 640x480; the menus are always 320x200
  pictures.
- **Low (320x200):** as the game starts.

In the game, keypad + and - still switch between them.

**Window size** (in a window only)

How big the window is, in whole multiples of the game's picture (320x200 or
640x480). "Largest that fits" picks the biggest that fits on your screen.

**4:3 (non-square pixels)** (on by default; with the original renderer, in a window at low resolution only)

The game's 320x200 pictures (the menus, and everything at low resolution) were
made for 1996 monitors, which showed them at 4:3, so everything looked a
little taller. Ticked, they are shown at about that shape: with the new
renderer by the whole numbers across and down that come nearest (on a
1920x1080 screen, 4 across by 5 down, which is 1280x1000), with the original
by stretching the height by 1.2. Unticked, pixels are square. At 640x480 the
picture already has the 4:3 shape.

**Full screen** (original renderer, in full screen only; not on Windows 95, 98 or ME)

How cnc-ddraw fills the screen with the original renderer. With the new
renderer this is greyed out: the picture is already the screen's own size,
and cnc-ddraw adds nothing.

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

### Smoothness

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

### Races

**Draw distance (experimental)**

How far races and replays are drawn:

- **The game's own** (the default): the in-game vision and texture distance
  sliders, as in 1996.
- **Furthest in one pass (25):** as far as the game's engine allows on its
  own: 25 landscape squares instead of the slider's 22, with textures all the
  way out instead of stopping at 14.
- **Double (44, experimental):** the ground goes on to 44 squares, drawn in
  extra passes beyond the engine's own limit. Trees, buildings and other
  scenery still appear at 25.

With either of the last two, the in-game sliders are ignored, and your own
settings are kept for when you go back. If something looks wrong (missing
patches of ground, cracks, flicker in the distance), go back to the game's own
and tell us.

**In-game render resolution** (new renderer only; not on Windows 95, 98 or ME)

How many pixels races and the attract-mode demo are drawn with:

- **Original** (the default): the game's own 640x480.
- **Max (experimental):** up to 1008x756, the most the game's 3D engine can
  draw. The size is the 4:3 picture that, enlarged by a whole number, fills
  the most of your screen or window, and never less than 640x480: 1008x756
  shown as it is on 1280x800 (Steam Deck), 1366x768 or 1600x900; 720x540
  doubled on 1920x1080; 960x720 doubled on 2560x1440 and tripled on
  3840x2160. The picture keeps the game's own view, with more detail. The
  dashboard (position, laps, map) is the game's own, enlarged by the game
  itself to match, so its edges may look a little uneven. Menus and
  split-screen races stay as they are.

If something looks wrong in a race, go back to Original and tell us.

## Audio

### Music

**CD music from ripped tracks**

The game's music was CD audio. Ticked, it's played from ripped tracks in the
music folder instead, with no CD needed. Put one file per CD track in the
folder, named by track number: track02.flac, Track02.ogg, 02.wav,
"02 - Title.flac" and so on (FLAC, Ogg Vorbis or 16-bit WAV). The window says
how many tracks it found.

**Volume**

The music's volume, on top of the game's own CD volume option.

**Folder and Browse**

Where the ripped tracks are: Music (a folder next to Big Red Racing.exe) by
default, or any folder you choose with Browse.

## Controller

**Configure**

Opens Windows' own game controller settings, where you can set up and
calibrate a joystick or pad (the Windows version of the game has no
calibration screen of its own; the DOS version does). Not shown under
Proton, where Steam sets up controllers.

The settings below are not on Windows 95, 98 or ME, which have no support
for modern pads.

**Modern pad (XInput)**

Xbox-style pads, including the Steam Deck's controls, work as the game's two
joysticks: the first pad you use becomes joystick 1 (JS1), the next one you use
joystick 2 (JS2), for two players. The left stick steers, and A and B are the
joystick's buttons. In the game's controls menu, each player chooses JS1 or JS2.
While a pad is connected, other joysticks (ones without XInput) are left out;
without one, they work as in 1996.

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

In a race, Start pauses it and carries on, as the Pause key does, and Back
(View on newer pads) ends it, as Esc does.

### Vibration

For a pad that can vibrate. The pad goes still whenever the race does, for
example when you pause.

**Enable vehicle damage vibration**

A short buzz from the pad whenever player one's vehicle is damaged in a crash
or a hard landing, and a longer one when it's wrecked; scraping along a wall
keeps it buzzing. It follows the game's own damage option, which is on unless
you turn it off in the game's menus: with damage off, vehicles take none, so
the pad stays still.

**Enable nitro boost vibration**

A steady, lighter rumble from the pad for as long as player one's nitro flames
show.

**Intensity**

How strong the damage buzz is, in percent of the pad's power (80% by
default); the nitro rumble is half as strong.

## Advanced

### Game

**Language**

The game's language, as in its window's Language menu: English (the default),
USA, French, German, Italian, Spanish or Japanese. It applies from the first
screen, "Enter your name". The window's Language menu still works, but only
until the game is closed: next time, this setting is used again.

### Clock

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

### Troubleshooting

**Log**

Writes BRRFIX.LOG next to Big Red Racing.exe: what was fixed and how the clock
ran. It's off by default; turn it on if you're reporting a problem, and send
the file with your report.

## The buttons

- **Skip this window next time:** starts the game straight away from now on.
  Hold Shift while starting, or start with -setup, to see this window again.
- **Patch the DOS version:** fixes the DOS version's RACING.EXE in the Game
  folder (the same game fixes, for DOS PCs and DOSBox), keeping the original as
  RACING.ORI. Press it again on a patched file to put the original back.
- **Help:** opens this file (or press F1).
- **Defaults:** puts every setting back to the defaults for your PC: Period
  defaults on Windows 95, 98 and ME, Modern defaults on anything newer (see
  General). Press Save or Play to keep them.
- **Play:** saves the settings and starts the game. When the game ends, this
  window comes back (on the same tab), so you can change something and play
  again; Quit leaves. Started with -play, or with this window skipped, the
  launcher ends with the game.
- **Save:** saves the settings without starting the game.
- **Quit:** closes the window without saving.

## The folders, and WINRACEP.EXE

Big Red Racing.exe and its files sit in a folder of their own, with the game
in a folder called Game inside it, and the CD music in a folder called Music.
(To use the game where it is instead, set [launcher] game= in BRRFIX.INI to
its folder, for example game=C:\BIGRED.)

The game's own files are left as they are. Windows 10 and 11 refuse to start
the 1996 WINRACE.EXE as it is: the linker that made it in 1996 wrote its file
headers carelessly, which Windows 95 didn't mind. So when you press Play, the
launcher makes WINRACEP.EXE ("P" for patched) beside itself: a copy of the
game's WINRACE.EXE with those headers corrected, which works on every version
of Windows, and using our PATCH.DLL (the fixes) where the game used its own
TIMER.DLL. It starts that copy in the Game folder. If the game's WINRACE.EXE
changes, the copy is made again.

## Starting from Steam

Add Big Red Racing.exe to Steam as a non-Steam game. Launch options:
**-play** skips the settings window, **-setup** always shows it. Any other
options are passed on to the game.

## Where the settings are kept

BRRFIX.INI (the fix's settings) and ddraw.ini (cnc-ddraw's, for full screen),
next to Big Red Racing.exe. Each setting is explained in the files, and some that the
window doesn't show can be changed there. The launcher changes only the values
it shows, and keeps everything else in the files as it was.
