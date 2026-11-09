"""dos-package: zip the DOS patch for players: our files only.

Writes work/release/brr-dos-patch-<version>.zip containing only BRRPATCH.EXE (the DOS
patcher, rebuilt by brrpatch-build), README.TXT (src/dospatch/README.txt, with the
version and RACING.EXE's checksum filled in) and LICENSE.TXT once the project has a
licence: 8.3 names, so the files copy straight onto a DOS PC. Text files get DOS line
endings. Checks first that data/dos-patches.json is up to date with src/dospatch/
(dos-build --check). BRRPATCH.EXE holds only our own bytes: brrpatch-build refuses any
16 bytes in a row that also appear in RACING.EXE (except plain text of our own matching a
common phrase), and every file in the zip is checked against the game manifests
(data/game-manifest.json). Needs Linux, Open Watcom and the Ghidra exports, as on the
development PC. Prints the zip's contents. It doesn't publish anything.
Example:
    python3 brr.py dos-package
"""

import hashlib
import json
import sys
import zipfile

import brrpatch_build
import dos_build
from common import DATA, WORK
from dropin_build import WATCOM
from dropin_package import LICENCES, ZIP_DATE, crlf, game_checksums
from prepkit import rel

HELP = "zip the DOS patch for players: BRRPATCH.EXE and README.TXT (checked against the game manifest)"

RELEASE = WORK / "release"


def shares_game_bytes(code, game, run=16):
    """(code, text): offsets in our code where 16 bytes in a row also appear in the game
    file. Matches that are plain text (our own messages, which can coincide with common
    phrases such as "not enough memory") are listed separately and allowed."""
    windows = {game[i:i + run] for i in range(0, len(game) - run + 1)}
    found = [i for i in range(0, len(code) - run + 1) if code[i:i + run] in windows
             and code[i:i + run] != bytes(run) and code[i:i + run] != b"\xcc" * run]
    text = [i for i in found if all(32 <= b < 127 for b in code[i:i + run])]
    return [i for i in found if i not in text], text


def can_rebuild():
    return (sys.platform.startswith("linux") and (WATCOM / "binl64" / "wcc386").exists()
            and (dos_build.EXPORTS / "calls.csv").exists())


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if not can_rebuild():
        print("dos-package: BRRPATCH.EXE is built with Open Watcom on Linux, from the Ghidra exports: "
              "package it on the development PC")
        return 1
    if dos_build.main(["--check"]):
        print("dos-package: run python3 brr.py dos-build first")
        return 1
    if brrpatch_build.main([]):
        return 1
    spec = json.loads((DATA / "dos-patches.json").read_text())
    version = spec["version"]
    readme = (dos_build.SOURCE / "README.txt").read_text().replace("@VERSION@", version)
    readme = readme.replace("@RACING_SHA256@", spec["input"]["sha256"])
    files = {"BRRPATCH.EXE": brrpatch_build.EXE.read_bytes(), "README.TXT": crlf(readme)}
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
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, ZIP_DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(f"  {rel(out)} ({out.stat().st_size:,} bytes):")
    for name, data in files.items():
        print(f"    {name:20} {len(data):>9,} bytes  sha256 {hashlib.sha256(data).hexdigest()[:16]}...")
    print(f"  checked against {len(known):,} game files: no matches"
          + ("" if licence else "; no LICENSE yet (the project's licence isn't chosen)"))
    print("dos-package: PASS")
    return 0
