"""dos-package: zip the DOS patch for players: our files only.

Writes work/release/brr-dos-patch-<version>.zip containing BRRPATCH.EXE (the DOS
patcher for MS-DOS 6 and later, rebuilt by brrpatch-build), README.TXT
(src/dospatch/README.txt, with the version and RACING.EXE's checksum filled in), the
Python patcher for Linux and macOS as PYTHON/DOSPATCH.PY with its data PYTHON/DOSPATCH.JSN
(scripts/dos_patch.py and data/dos-patches.json), and LICENSE.TXT (the GNU GPL, version 3,
from LICENSE): 8.3 names throughout, so the files copy straight onto a DOS PC. Text files get
DOS line endings. On the development PC it first checks that data/dos-patches.json is up
to date with src/dospatch/ (dos-build --check, which needs the Ghidra exports); elsewhere
it says so and packages the committed data. Our bytes only: brrpatch-build refuses any 16
bytes in a row of BRRPATCH.EXE's data that also appear in RACING.EXE, the same check is
made on the Python patch data's code (plain text of our own matching a common phrase is
allowed and listed), and every file in the zip is checked against the game manifests
(data/game-manifest.json). Prints the zip's contents. It doesn't publish anything.
Example:
    python3 brr.py dos-package
"""

import hashlib
import json
import zipfile

import brrpatch_build
import dos_build
from common import DATA, ROOT, WORK
from dropin_build import watcom_bin
from dropin_package import LICENCES, crlf, game_checksums, zip_date
from prepkit import rel

HELP = "zip the DOS patch for players: BRRPATCH.EXE, README.TXT and the Python patcher"

RELEASE = WORK / "release"


def shares_game_bytes(code, game, run=16):
    """(code, text): offsets in our code where 16 bytes in a row also appear in the game
    file. Filler is skipped: runs where all but at most two bytes are the same value (zeroed
    variables after a short constant such as "\\r\\n", padding, 0xCC, an 8-bit sound's silence),
    which carry no content. Matches
    that are plain text (our own messages, which can coincide with common phrases such as
    "not enough memory") are listed separately and allowed."""
    windows = {game[i:i + run] for i in range(0, len(game) - run + 1)}

    def filler(window):
        return max(window.count(b) for b in set(window)) >= run - 2

    found = [i for i in range(0, len(code) - run + 1) if code[i:i + run] in windows
             and not filler(code[i:i + run])]
    text = [i for i in found if all(32 <= b < 127 for b in code[i:i + run])]
    return [i for i in found if i not in text], text


def can_check_sources():
    """dos-build --check needs Open Watcom and the Ghidra exports."""
    return (watcom_bin() / "wcc386").exists() and (dos_build.EXPORTS / "calls.csv").exists()


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if can_check_sources():
        if dos_build.main(["--check"]):
            print("dos-package: run python3 brr.py dos-build first")
            return 1
    else:
        print("dos-package: NOTE: can't check data/dos-patches.json against src/dospatch/ here (that "
              "needs the Ghidra exports): packaging the committed data. Make releases on the "
              "development PC.")
    if brrpatch_build.main([]):
        return 1
    spec = json.loads((DATA / "dos-patches.json").read_text())
    version = spec["version"]
    code = bytes.fromhex(spec["code"]["hex"])
    shared, text = shares_game_bytes(code, dos_build.GAME.read_bytes())
    if shared:
        print(f"dos-package: REFUSED: the patch's code shares 16 or more bytes in a row with "
              f"RACING.EXE at code offsets {shared[:8]}")
        return 1
    readme = (dos_build.SOURCE / "README.txt").read_text().replace("@VERSION@", version)
    readme = readme.replace("@RACING_SHA256@", spec["input"]["sha256"])
    files = {"BRRPATCH.EXE": brrpatch_build.EXE.read_bytes(), "README.TXT": crlf(readme),
             "PYTHON/DOSPATCH.PY": (ROOT / "scripts" / "dos_patch.py").read_bytes(),
             "PYTHON/DOSPATCH.JSN": (DATA / "dos-patches.json").read_bytes()}
    licence = next((path for path in LICENCES if path.exists()), None)
    if licence:
        files["LICENSE.TXT"] = crlf(licence.read_text())
    known = game_checksums()
    refused = [f"{name} matches {known[digest]}" for name, data in files.items()
               if (digest := hashlib.sha256(data).hexdigest()) in known]
    if refused:
        print("dos-package: REFUSED, game content: " + "; ".join(refused))
        return 1
    RELEASE.mkdir(parents=True, exist_ok=True)
    out = RELEASE / f"brr-dos-patch-{version}.zip"
    date = zip_date()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, date)
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(f"  {rel(out)} ({out.stat().st_size:,} bytes):")
    for name, data in files.items():
        print(f"    {name:20} {len(data):>9,} bytes  sha256 {hashlib.sha256(data).hexdigest()[:16]}...")
    phrases = sorted({code[i:i + 16].decode() for i in text})
    print(f"  checked against {len(known):,} game files: no matches"
          + (f"; the Python patch data's code shares no 16-byte run with RACING.EXE except our own "
             f"text ({', '.join(repr(p) for p in phrases)})" if phrases else "")
          + ("" if licence else "; no LICENSE yet (the project's licence isn't chosen)"))
    print("dos-package: PASS")
    return 0
