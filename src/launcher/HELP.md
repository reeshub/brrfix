# Big Red Launcher @VERSION@ by ctrl-alt-rees

ctrl-alt-rees.com

Big Red Launcher fixes and starts the Windows version of Big Red Racing (1996),
and lets you choose its settings. This file explains every setting
in its window. Hover over a setting to see a one-line reminder.

Start the game with **Big Red Racing.exe**. To see the settings window again
after ticking "Skip this window next time", hold **Shift** while starting it,
or start it with **-setup**.

The settings are on six tabs: General, Display, Renderer, Audio, Controller
and Advanced.
Click a tab, or use the arrow keys while the tabs are selected (as they are
when the window opens). This file follows the same order.

## General

Who made Big Red Launcher, what it does, and two sets of settings to start
from:

- **Period defaults:** for a 1990s PC with Windows 95, 98 or ME, where the
  game's 640x480 3D is heavy work: full screen at the game's low resolution
  (320x200), drawn by the original renderer, with interpolation on and the
  framerate cap at the display rate.
- **Modern defaults:** for today's Windows, Linux and Steam Deck: full screen
  at high resolution (640x480), drawn sharp in full colour by the Modern
  renderer, with modern pads on.

Each button sets every setting on every tab. Press Save or Play to keep them.

## Display

The Display tab chooses the renderer (the first setting below) and shows its
display settings; the Renderer tab, after it, has the rest of its settings.
Each renderer has its own:

- **Original** is the 1996 game's own drawing, as close to what players saw
  then as possible, with only the smooth-motion fixes and cnc-ddraw's
  full-screen scaling added. It stays as it is from now on.
- **Modern** is where the improvements go: sharp at any size, full colour
  with smoother gradients, further draw distances, more pixels in races.

Choices marked **(1996)** are the game's own, as it was in 1996.

### Display

**Renderer** (not on Windows 95, 98 or ME, which always use the original)

- **Modern** (the default): each picture goes to cnc-ddraw at its own size,
  crisp and with no black borders of its own: the menus as their 320x200
  pictures, races as 640x480 (or bigger, with Max). cnc-ddraw then enlarges
  it to the screen or the window, as Scaling (below) says, 4:3 sharp by
  default, so the menus fill the same space as the races. Starting and
  leaving a race changes the picture's size, which cnc-ddraw may show as a
  brief jump. The race dashboard (map, place, nitro, laps, damage and
  messages) is the 1996 game's low-resolution art, drawn by the game on a
  layer of its own and enlarged by whole numbers, so it stays sharp and in
  its corner at any race size. It needs cnc-ddraw (CNCDD.DLL, next to Big Red Racing.exe), as
  the game does on modern Windows; without it, the original is used.
- **Original:** the game's own drawing, exactly as in 1996. In full screen
  cnc-ddraw enlarges it (see "Full screen" below); the menus are 320x200
  pictures doubled into the middle of a 640x480 picture, with black bands
  above and below. The draw distance is the in-game sliders', and a window
  is the game's own size.

**Mode**

- **Full screen** (the default): the game fills the screen, without asking.
- **Windowed:** with the Modern renderer, a plain window (no menu bar) that
  cnc-ddraw draws; press **Alt+Enter** to switch it to full screen and back.
  With the Original renderer, the game's own 1996 window, with its menu bar
  (About, Modes, Language), as on a Windows 95 desktop, at the game's own
  size.

To switch between a window and full screen otherwise, quit and choose the
other mode here: the game's own switch (the Modes menu's Full screen item, and
Tab) broke its window, so it's removed.

**Resolution**

- **Low (320x200) (1996):** as the game starts.
- **High (640x480) (1996)** (the Original renderer's default): the game's own
  sharper mode, which it only offered through its window's Modes menu or
  keypad +. It applies from the first screen. Races are drawn at 640x480; the
  menus are always 320x200 pictures.
- **Max** (Modern renderer only, and its default): races and the attract-mode
  demo drawn with as many pixels as your screen (or window) is tall, up to
  the Max limit below, at your screen's shape (native widescreen, below) or
  4:3, and never less than 640x480: 1280x800 on the Steam Deck, 1920x1080
  on a 1080p screen. cnc-ddraw then enlarges it if need be, as Scaling says.
  The dashboard (position, laps, map) is laid over it in whole-number blocks
  (see Modern, above). Menus and split-screen races stay as they are.

**Max limit** (Modern renderer, with Max)

How many rows Max draws races with at most:

- **Screen height** (the default): your screen's (or window's) own height,
  the sharpest. The game draws in software, on the processor, so every extra
  row costs frame rate: on a 4K screen even a fast PC manages only about 25
  frames a second, so pick 1080 there.
- **1080:** smooth on any modern PC. A 4K screen shows it doubled exactly.
- **1440:** sharper on 1440p and 4K screens; needs a fast PC.

With native widescreen (below) the limit is the race's height, and its width
follows the screen's shape: 1920x1080 at 1080.

**Native widescreen** (Modern renderer, with Max; on by default)

In full screen on a screen wider than 4:3 (16:10 like the Steam Deck, 16:9,
or wider), races are drawn at your screen's own shape: 1280x800 on the Steam
Deck, 1920x1080 on a 1080p screen (and, with the 1080 limit, on 1440p and 4K
screens, enlarged exactly). The game sees as much from top to bottom as in
4:3 and more at the sides; nothing is stretched, and the dashboard stays in
the corners. The menus stay 4:3, enlarged by whole numbers in the middle of a
picture of your screen's shape, which cnc-ddraw enlarges to fill the screen's
height, with their backgrounds across the whole screen (Widescreen menu
backgrounds, below) or black bars at the sides. Unless the screen's height is a
multiple of 200 (800 on the Steam Deck), the picture changes size between the
menus and a race, which cnc-ddraw may show as a brief flicker.

The game only works out the ground it needs for a 4:3 view, in a grid of
limited size; it's widened to match, up to that grid's edge, and the draw
distances chosen here (Race world) fill in the rest. With the in-game sliders
on ultrawide screens, the farthest ground at the far left and right may be
missing. In a window (which is 4:3), on a 4:3 screen and in split-screen
races the picture stays 4:3.

With the Original renderer, keypad + and - still switch between high and low
resolution in the game, as in 1996. The Modern renderer keeps the one chosen
here.

**Scaling** (not on Windows 95, 98 or ME)

How cnc-ddraw enlarges the picture: the Original renderer's in full screen,
the Modern renderer's in full screen and in its window. Each renderer keeps
its own choice: Integer for the Original, 4:3 sharp for the Modern one.

- **Integer (sharp):** the largest whole-number size that fits, with square
  pixels and black borders. 4x on the Steam Deck, which fills its screen.
  At 640x480 a screen needs to be at least 960 pixels tall for 2x; on
  smaller screens the picture is shown at 1x, so 4:3 sharp may suit them
  better.
- **4:3 sharp:** the 1996 monitor shape, as big as fits, kept sharp (the
  menus' 320x200 pictures a little taller, as 1996 monitors showed them).
- **4:3 nearest:** the same shape with plain pixels; some rows and columns are
  one screen pixel wider than others.
- **Stretch:** fills the whole screen, whatever its shape.
- **Smooth:** slightly blurred, as cnc-ddraw does by default.
- **Square sharp (full height):** the picture's own shape, with square
  pixels, as tall as the screen and kept sharp: the menus' 320x200 pictures
  are wider than 4:3 (they fill a 16:10 screen such as the Steam Deck's),
  races' 4:3 pictures have black bars at the sides.

With native widescreen the pictures already have the screen's shape (the
menus 4:3 inside them), so the 4:3 choices keep the picture's own shape
instead: 4:3 sharp is then simply sharp, 4:3 nearest plain pixels.

**Window size** (Modern renderer, in a window only)

How big the window is, in whole multiples of 640x480. "Largest that fits"
picks the biggest that fits on your screen. The picture is enlarged into it
as Scaling says.

**Colour depth** (Modern renderer)

- **Full colour** (the default): the picture is made in full colour from the
  game's own 256, which allows the smoother gradients (Renderer tab, Full
  colour).
- **256 colours (1996):** the game's own colours, as it drew them.

### Frame rate

**Framerate cap**

The most pictures drawn each second. "Display rate" matches your screen's
refresh rate, which is usually best. "Unlocked" draws as many as your PC can.
The game itself runs at the same speed whatever you choose.

**Vsync**

Waits for the screen to finish each refresh before showing the next picture,
so there are no horizontal tears when the camera turns. It can add a little
input lag.

## Renderer

How the race world and the pictures are drawn. With the Original renderer
(and on Windows 95, 98 and ME) only Smoothness and Colour animation are here.

### Smoothness

**Interpolation (smooth motion)**

The game's physics moves the vehicles 35 times a second. With interpolation,
the vehicles and the camera are drawn in between those steps, so a modern PC
shows smooth motion at its full frame rate instead of 35 jerky steps a second.
The physics itself is exactly the same either way.

**Camera smoothing**

Experimental: softens the camera's movement further. The camera stutter is
already fixed without it, so leave it Off unless you want to experiment.

### Full colour (Modern renderer)

Both need Colour depth: Full colour (Display tab); with 256 colours they're
greyed out.

**Smoother sky gradients** (full colour; on by default)

The sky as a smooth gradient, worked out for every pixel, instead of the
game's 16 flat bands of colour.

**Smoother game object gradients** (full colour; on by default)

Lit surfaces (buildings, trees, vehicles, tunnel walls) shaded smoothly. The
game shades them a whole palette colour at a time, which shows as stripes;
the shade in between is worked out for every pixel instead.

### Race world (Modern renderer)

**Draw distance**

How far the ground, trees, buildings and the other vehicles are drawn:

- **In-game sliders (1996):** the vision and texture distance sliders in the
  game's options menu, as in 1996.
- **Engine max (25):** as far as the game's engine draws on its own: 25
  landscape squares instead of the slider's 22. In widescreen the sides it
  can't hold are filled in by extra passes.
- **Far (44):** the ground, scenery and vehicles go on to 44 squares, the
  ground beyond 25 drawn in extra passes.
- **Max out (60)** (the default): as far as the engine's numbers allow.

The further, the more there is to draw: it costs frame rate, as the game
draws in software. With any but the first, the in-game sliders are ignored,
and your own settings are kept for when you go back.

**Textures** (with a draw distance chosen here)

How far the ground is textured: **As draw distance** (the default), **Engine
max (25)**, or the options menu's **In-game slider (1996)** (which stops at 14).
Beyond it the ground is plain shaded colour, which is quicker to draw.

**Object lighting**

How trees, buildings and vehicles are lit by the level's sun:

- **Flat** (the default): by face, everywhere, so an object looks the same
  near and far.
- **Smooth:** from their corners, everywhere.
- **Original (1996):** smooth close up and flat (by face) beyond 8 squares,
  as in 1996. Objects change as they cross that distance, and the smooth look is
  darker on boxy shapes, so they seem to go dark as they come near.

**Object detail**

The game keeps up to six versions of each model, simpler with distance (a
fence is a thin board far away and has thickness close up), and doesn't draw
it at all beyond the last, usually 39 squares.

- **Highest** (the default): always the most detailed version, at any
  distance. There's more to draw, though in our tests on Italy it made no
  measurable difference to how long each picture took.
- **Matched:** each version is picked by the object's size on screen, as the
  game picked it for its 640x480 picture. At today's sizes the changes happen
  further away, where they can't be seen, and objects reach the far draw
  distances.
- **Original (1996):** by distance, as in 1996: at today's sizes fences and
  barriers visibly change shape close up, and objects stop at 39 squares.

### Miscellaneous

**Colour animation (as in DOS)** (on by default; both renderers)

Four levels cycle some of their colours: CHOPPER3, CITY4 (Italy), MOON2 and
RIG4. The DOS version does it; the Windows version lost it, as the code that
did it was left uncalled. Ticked, those colours move again, at the DOS
version's speed, while the race runs (they stop when you pause, as in DOS).

**4:3 HUD correction** (Modern renderer; on by default)

The race dashboard (map, place, nitro, laps, damage and messages) is the
game's 320x200 art, enlarged by whole numbers. Ticked, each of its pixels is
a little taller than wide when the enlargement allows it (3x4 screen pixels
with the Large size on the Steam Deck, 4x5 at 1080 rows), as the 320x200 art
looked on a 1996 4:3 monitor. Unticked, square: a little wider, as the art is
in the files. At the Small size's 2x2 there's no whole-number 4:3 shape, so
it's square either way.

**HUD size** (Modern renderer)

- **Small** (the default): about half the 1996 size: 2 screen pixels for
  each of the art's on 800 and 900 rows, 3 at 1080, 4 at 1440.
- **Large (1996):** as big as the game's low resolution showed it (4 on the
  Steam Deck, 5 at 1080).

The track map is enlarged twice as much as the rest (the same 320x200 art:
the game's 640x480 map was scaled badly in places in 1996).

Either way the dashboard is laid out afresh: the map at the top left and the
place at the top right sit the same distance from their edges, the nitro and
the lap counter line up under the place with even gaps, and the messages
(DAMAGE, FINAL LAP, WRONG WAY) are centred.

**Widescreen menu backgrounds** (Modern renderer, with Max, Native widescreen
and full screen; on by default)

With native widescreen the menus stay 4:3 in the middle of the screen.
Ticked, each menu's background picture is enlarged to the screen's whole
width instead of leaving black bars, sharply (each of its pixels repeated),
with the menu's text, buttons and 3D drawn over it where they always were.
As the picture is wider than it was, some of it is cropped: the top for
most menus; the top and bottom on the globe where you choose a race (whose
globe and lettering are drawn round, with square pixels); and on each
level's picture whichever part keeps the level's title in view. The mouse
pointer stays inside the menu, so it's never cut off at its sides. On the
car screen the car is drawn a little smaller, as its left side used to be cut
off where the 1996 screen ended. Unticked: black bars at the sides.

### Engine fixes (Modern renderer; all on by default)

Three of the 1996 3D engine's own faults, which show more at today's
resolutions and draw distances. Untick one to see the game as it was.

**Steady ground textures**

The game drew a ground square one way close up (with perspective) and a
cheaper way from 4 squares out, and the texture jumped where they met: banks
beside the road twitched as you came near. Every textured square is now
drawn the first way, more precisely than the engine managed. About 1 ms a
frame more.

**Better draw order**

The ground is drawn without the engine's depth buffer, so only the order
things are drawn in keeps an object behind a hill. The game ordered objects
by a sphere that counts their height, so a tall tree could be drawn as if two
squares nearer and show through the hill in front of it. Objects are now
ordered by their footprint.

**Close gaps between polygons**

The level designers' editor placed about half the pieces meant to meet (such
as Italy's tunnel sections and the billboards at the start) one unit apart, a
slit of sky at today's resolutions; they're now drawn together. Polygon edges
are also worked out exactly, where the engine lost a little along long
edges. Some slits remain where separate pieces meet at an angle.

**Horizon culling (experimental)** (off by default; needs Close gaps)

The engine paints the scene from far to near, so the ground and objects
behind a hill are painted and then covered. Ticked, anything nearer ground
will surely cover is left out: the picture is exactly the same. So far it
saves about as much time as it takes to work out, because hidden far ground
is cheap for the engine to paint; it's here to try, and a faster version
(skipping hidden ground before the engine even prepares it) is planned.


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

### Sound effects

**Distant vehicles quieter** (on by default)

The game fades other vehicles' sounds with their distance from the camera:
full volume within about a square and a half, nothing beyond 16 squares. On
Windows that never worked: the game gave DirectSound its volumes in the wrong
units, so every sound played almost at full volume, however far away. Ticked,
the volumes are converted properly, as the DOS version plays them, so far
trucks are quieter. This also brings the game's own effects volume (its sound
options screen) to life, which barely changed anything before, and the game's
three engine sounds go to the nearest trucks rather than the first three in
its list. Unticked: as the 1996 Windows version.

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

### Menus

**Controller menu navigation** (on by default; both renderers, any joystick)

The 1996 game already lets joystick button 1 (A on a pad) choose in its menus,
as Return does. With this ticked, button 2 (B) goes back, as Esc does (on the
first screens, as with Esc, it quits the game), and the stick's left and right
(and a pad's D-pad) change the choice where the menus have arrows, as the arrow
keys do: the race, the logo, the car. Not on the keys screen, where a
joystick's buttons and directions can be given to controls, nor the joystick
screen. Moving up and down through a menu's options is still to come.

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
