Big Red Racing DOS patch
========================

A patcher for the DOS version of Big Red Racing (1996), v2.24 (from the
official v2.25 patch). It reads your own RACING.EXE and writes a fixed copy,
BRRFIX.EXE, beside it, for original PCs and for DOSBox. Your RACING.EXE is
never changed.

What it fixes
-------------
- No more camera stutter: the 3D engine could only tilt and turn the view in
  steps of 0.35 degrees, so the picture jolted up and down as the camera moved,
  and the camera's lean flicked with every step. The race is now drawn with
  finer angles and a steady lean.
- Smooth movement on fast machines: the game's physics runs 35 times a second;
  the trucks and the follow camera are now drawn between physics steps, so a
  fast machine shows 70 different pictures a second instead of 35.
- Tear-free frames on fast machines: each frame is shown at the start of the
  screen's vertical retrace (once per refresh: 70 Hz at 320x200).
- A shorter catch-up after a hiccup on fast machines: at most 8 ticks (about a
  tenth of a second) of game time in one frame, instead of 35.
- Two camera calculations that depended on the frame rate.
- The physics is left exactly as it was: the same game, bit for bit.

"Fast machine" means one that draws a frame in 3 ticks (3/70 s) or less, on
average: a fast Pentium, or DOSBox with plenty of cycles. On slower machines,
such as a 486, the patched game runs exactly as fast as the original, with the
same detail: there's nothing to smooth between, and waiting for the screen would
only cost time. The camera fixes (finer angles, steady lean) apply on every
machine. The patch's own code uses
only 386/486 instructions and whole-number arithmetic (no maths coprocessor
needed, like the game).

You need
--------
- Your own copy of Big Red Racing for DOS, installed and updated with the
  official v2.25 patch. Check: RACING.EXE should be 629,472 bytes, SHA-256
  @RACING_SHA256@
  (The patcher refuses any other file, with a message.)
- A modern PC with Python 3.8 or newer (Windows, Linux or macOS) to run the
  patcher. The patched game then runs on the DOS machine without Python.

Patching
--------
1. Unzip this folder anywhere.
2. Run the patcher on your RACING.EXE, for example on Windows:
       py dos_patch.py C:\BIGRED\RACING.EXE
   or on Linux and macOS:
       python3 dos_patch.py ~/bigred/RACING.EXE
   It writes BRRFIX.EXE in the same folder (or choose with -o NAME).
3. Optional: check the result:
       python3 dos_patch.py --verify C:\BIGRED\BRRFIX.EXE
Options (all fixes are on unless you turn them off):
   --no-camera --no-interpolation --no-fine-angles --no-roll-fix --no-retrace
   --cap N      the catch-up cap on fast machines, 1 to 35 ticks (default 8)
   python3 dos_patch.py --help lists everything.

On an original PC
-----------------
1. Copy BRRFIX.EXE (about 645 KB: it fits on a floppy disk) into the game's
   folder, for example C:\BIGRED, next to RACING.EXE and DOS4GW.EXE.
2. Start it with BRRFIX instead of RACING. Everything else is the same: the CD,
   the sound settings, your saved players, and switches such as -DIAGS.
3. The main menu shows the version and the fixes chosen at the bottom right,
   for example "V2.24R F0.1 CIARV": patch 0.1 with the Camera, Interpolation,
   fine Angles, Roll and retrace (V) fixes on.
Modem play (MODEM.BAT) still starts the original RACING.

In DOSBox
---------
Copy BRRFIX.EXE into the game folder you mount as C:, and run BRRFIX. Give
DOSBox plenty of CPU (for example cycles=max, or a fixed number high enough
for 70 frames a second) to get the smooth picture.
DOSBox shows the game's 70 Hz picture on your monitor's refresh rate: on a
60 Hz monitor some frames are always shown twice, whatever the game does, which
looks like a slight judder. A 120 Hz, 144 Hz or variable-refresh (G-Sync,
FreeSync) display helps.

Going back to the original
--------------------------
Delete BRRFIX.EXE and start RACING as before: RACING.EXE was never changed.

What's in this zip
------------------
- dos_patch.py        the patcher (Python, standard library only)
- dos-patches.json    the patch: our own compiled code, where it goes, and
                      checksums of the places it changes. No part of the game.
- README.txt          this file

About
-----
Made for the Big Red Racing preservation project, with the blessing of Paul
and Peter Ranson of Big Red Software. Patch version @VERSION@.
