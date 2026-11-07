Big Red Racing drop-in fix
==========================

A replacement TIMER.DLL for the Windows 95 version of Big Red Racing (1996),
patched to v2.25. It makes the original game run on modern Windows and on
Linux/Steam Deck through Proton, and makes it run smoothly:

- No more "privileged instruction" crash: the game's direct reads of the 1996
  VGA card are replaced.
- A steady clock at exactly 70 ticks a second, as the DOS version had (the
  original Windows version ran 2% fast, at 71.4).
- Smooth movement at any refresh rate: the trucks and the follow camera are
  drawn between the game's physics steps, which happen 35 times a second.
- Two camera calculations that depended on the frame rate are fixed.
- The physics is left exactly as it was: the same game, bit for bit.

It contains no part of the game. It changes the game only in memory while it
runs, never on disk, and only after checking that WINRACE.EXE is the exact
v2.25 file and that every place it changes is as expected. If anything doesn't
match, it changes nothing, writes why to BRRFIX.LOG, and works like the
original TIMER.DLL.

You need
--------
Your own copy of Big Red Racing, installed and updated to v2.25 with the
official patch. Check: WINRACE.EXE should be 534,016 bytes, SHA-256
@WINRACE_SHA256@

Install
-------
1. In the game folder (for example C:\BIGRED), rename the game's own TIMER.DLL
   to TIMER.DLL.ORIGINAL, so you can put it back later.
2. Copy TIMER.DLL and BRRFIX.INI from this zip into the game folder.
3. Run WINRACE.EXE as usual.
To undo: delete TIMER.DLL and BRRFIX.INI, and rename TIMER.DLL.ORIGINAL back
to TIMER.DLL.

Steam Deck and Linux (Proton)
-----------------------------
1. Install the fix as above.
2. In Steam: Games > Add a Non-Steam Game to My Library > Browse, choose
   WINRACE.EXE.
3. Right-click it > Properties > Compatibility > "Force the use of a specific
   Steam Play compatibility tool", and pick a Proton version.
4. Play. To see Proton's log, add PROTON_LOG=1 %command% to the launch options.

Settings: BRRFIX.INI
--------------------
Every fix can be switched off, and the tick rate, the frame limit and the
logging can be changed. The file explains each setting. If it's missing, the
defaults are used: all fixes on, 70.0 ticks a second, frames at the display's
refresh rate.

Troubleshooting
---------------
- The picture is tiny in a corner of a black screen, or the screen mode won't
  change: modern Windows and Wine handle the game's 1996 320x200 full-screen
  mode badly. The free, open-source DirectDraw wrapper cnc-ddraw fixes this:
  download it from https://github.com/FunkyFr3sh/cnc-ddraw/releases, put its
  ddraw.dll and ddraw.ini next to WINRACE.EXE, and set maintas=true in
  ddraw.ini to keep the original 4:3 shape. Under Wine or Proton, also set the
  DLL override ddraw=n,b (Proton: WINEDLLOVERRIDES="ddraw=n,b" %command%).
- "This program requires 256 colour mode to run windowed": answer OK to run
  full screen.
- Something wrong? Look in BRRFIX.LOG in the game folder: it says what was
  checked and changed. Set a fix to 0 in BRRFIX.INI to rule it out.

About
-----
Made for the Big Red Racing preservation project, with the blessing of Paul
and Peter Ranson of Big Red Software. Version @VERSION@.
