Big Red Racing DOS patch
========================

BRRPATCH.EXE fixes the DOS version of Big Red Racing (1996), v2.24 (from the
official v2.25 update), for original PCs and for DOSBox. It patches the game's
RACING.EXE where it is, and keeps the original as RACING.ORI, so you can put
it back at any time.

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
same detail. The camera fixes apply on every machine. The patch's own code
uses only 386/486 instructions and whole-number arithmetic.

You need
--------
Your own copy of Big Red Racing for DOS, installed and updated with the
official v2.25 update. Check: RACING.EXE should be 629,472 bytes, SHA-256
@RACING_SHA256@
BRRPATCH refuses any other file, and changes nothing.

Patching on a DOS PC (or a Windows 95/98/ME MS-DOS Prompt)
---------------------------------------------------------
1. Copy BRRPATCH.EXE into the game's folder, for example C:\BIGRED, next to
   RACING.EXE. It fits on a floppy disk.
2. In that folder, type:
       BRRPATCH
   (or BRRPATCH C:\BIGRED from anywhere). Checking and patching take a few
   seconds on a 486.
3. Start the game as usual, with RACING. The main menu shows the patch at the
   bottom right, for example "V2.24R F0.1 CIARV": patch 0.1 with the Camera,
   Interpolation, fine Angles, Roll and retrace (V) fixes on.
It needs about 650 KB of free disk space while it works.

To put the original back:
       BRRPATCH /U
BRRPATCH /? lists the options.

In DOSBox
---------
Copy BRRPATCH.EXE into the folder you mount as C:, then run BRRPATCH and
RACING in DOSBox as above. Give DOSBox plenty of CPU (for example cycles=max,
or a fixed number high enough for 70 frames a second) to get the smooth
picture. DOSBox shows the game's 70 Hz picture on your monitor's refresh rate:
on a 60 Hz monitor some frames are always shown twice, whatever the game does,
which looks like a slight judder. A 120 Hz, 144 Hz or variable-refresh
(G-Sync, FreeSync) display helps.

On modern Windows
-----------------
BRRPATCH.EXE is a DOS program, so 64-bit Windows can't run it. Use the
Windows version's launcher instead (Big Red Racing.exe, from the drop-in fix
download): its "Patch the DOS version" button does the same, and pressed again
puts the original back. Or run BRRPATCH in DOSBox.

What's in this zip
------------------
- BRRPATCH.EXE   the patcher (a DOS program; no DOS extender needed). It holds
                 only our own code and the list of changes: no part of the
                 game. It checks your RACING.EXE before and after patching.
- README.TXT     this file

About
-----
Made for the Big Red Racing preservation project, with the blessing of Paul
and Peter Ranson of Big Red Software. Patch version @VERSION@.
