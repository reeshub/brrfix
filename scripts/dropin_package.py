"""dropin-package: zip the drop-in fix for players: our files only.

Writes work/release/brr-dropin-fix-<version>.zip containing TIMER.DLL (ours, from
work/build/dropin/ - run dropin-build first), BRRFIX.INI (the defaults, with comments),
README.txt (install, Proton, troubleshooting) and LICENSE.txt once the project has a
licence. Text files get Windows line endings. Every file is checked against the game
manifests (data/game-manifest.json): any file whose SHA-256 matches a game file is
refused. Prints the zip's contents. It doesn't publish anything.
Example:
    python3 brr.py dropin-package
"""

import hashlib
import json
import re
import zipfile

from common import DATA, ROOT, WORK
from prepkit import rel

HELP = "zip the drop-in fix for players (our files only, checked against the game manifest)"

SOURCE = ROOT / "src" / "dropin"
BUILD = WORK / "build" / "dropin"
RELEASE = WORK / "release"
MANIFEST = DATA / "game-manifest.json"
LICENCES = [ROOT / "LICENSE", ROOT / "LICENSE.txt", ROOT / "LICENSE.md"]
# A fixed date for every entry, so the zip is the same each time it's built.
ZIP_DATE = (2026, 1, 1, 0, 0, 0)


def game_checksums():
    manifest = json.loads(MANIFEST.read_text())
    return {entry["sha256"]: f"{tree}/{name}" for tree, files in manifest.items()
            for name, entry in files.items()}


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    dll = BUILD / "TIMER.DLL"
    if not dll.exists():
        print("dropin-package: build it first: python3 brr.py dropin-build")
        return 1
    version = re.search(r'BRRFIX_VERSION "([^"]+)"', (SOURCE / "brrfix.h").read_text()).group(1)
    winrace = json.loads(MANIFEST.read_text())["game"]["WINRACE.EXE"]["sha256"]
    readme = (SOURCE / "README.txt").read_text().replace("@VERSION@", version)
    readme = readme.replace("@WINRACE_SHA256@", winrace)
    files = {"TIMER.DLL": dll.read_bytes(),
             "BRRFIX.INI": crlf((SOURCE / "BRRFIX.INI").read_text()),
             "README.txt": crlf(readme)}
    licence = next((path for path in LICENCES if path.exists()), None)
    if licence:
        files["LICENSE.txt"] = crlf(licence.read_text())
    known = game_checksums()
    refused = [f"{name} matches {known[digest]}" for name, data in files.items()
               if (digest := hashlib.sha256(data).hexdigest()) in known]
    if refused:
        print("dropin-package: REFUSED, game content: " + "; ".join(refused))
        return 1
    RELEASE.mkdir(parents=True, exist_ok=True)
    out = RELEASE / f"brr-dropin-fix-{version}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, ZIP_DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    print(f"  {rel(out)} ({out.stat().st_size:,} bytes):")
    for name, data in files.items():
        print(f"    {name:12} {len(data):>8,} bytes  sha256 {hashlib.sha256(data).hexdigest()}")
    print(f"  checked against {len(known):,} game files: no matches"
          + ("" if licence else "; no LICENSE yet (the project's licence isn't chosen)"))
    print("dropin-package: PASS")
    return 0
