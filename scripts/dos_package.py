"""dos-package: zip the DOS patch for players: our files only.

Writes work/release/brr-dos-patch-<version>.zip containing only the standalone
patcher (dos_patch.py), its patch data (data/dos-patches.json, from dos-build) and
README.txt (src/dospatch/README.txt, with the version and RACING.EXE's checksum filled
in), plus LICENSE.txt once the project has a licence. Text files get Windows line
endings. Every file is checked against the game manifests (data/game-manifest.json):
any file whose SHA-256 matches a game file is refused. Also checks that the patch data
is up to date with the sources (dos-build --check) and holds no game bytes: none of
its code matches a stretch of 16 or more bytes of RACING.EXE (except plain text of
our own that happens to match a common phrase, which it lists). Prints the zip's
contents. It doesn't publish anything.
Example:
    python3 brr.py dos-package
"""

import hashlib
import json
import zipfile

import dos_build
from common import DATA, ROOT, WORK
from dropin_package import LICENCES, MANIFEST, ZIP_DATE, crlf, game_checksums
from prepkit import rel

HELP = "zip the DOS patch for players (our files only, checked against the game manifest)"

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


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if dos_build.main(["--check"]):
        print("dos-package: run python3 brr.py dos-build first")
        return 1
    spec = json.loads((DATA / "dos-patches.json").read_text())
    version = spec["version"]
    game = dos_build.GAME.read_bytes()
    code = bytes.fromhex(spec["code"]["hex"])
    shared, text = shares_game_bytes(code, game)
    if shared:
        print(f"dos-package: REFUSED: the patch's code shares 16 or more bytes in a row with "
              f"RACING.EXE at code offsets {shared[:8]}")
        return 1
    readme = (dos_build.SOURCE / "README.txt").read_text().replace("@VERSION@", version)
    readme = readme.replace("@RACING_SHA256@", spec["input"]["sha256"])
    files = {"dos_patch.py": (ROOT / "scripts" / "dos_patch.py").read_bytes(),
             "dos-patches.json": (DATA / "dos-patches.json").read_bytes(),
             "README.txt": crlf(readme)}
    licence = next((path for path in LICENCES if path.exists()), None)
    if licence:
        files["LICENSE.txt"] = crlf(licence.read_text())
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
    phrases = sorted({code[i:i + 16].decode() for i in text})
    print(f"  checked against {len(known):,} game files: no matches; the patch's "
          f"{len(code):,} bytes of code share no 16-byte run with RACING.EXE"
          + (f" except our own text matching common phrases ({', '.join(repr(p) for p in phrases)})"
             if phrases else "")
          + ("" if licence else "; no LICENSE yet (the project's licence isn't chosen)"))
    print("dos-package: PASS")
    return 0
