"""dropin-package: zip the drop-in fix for players: our files only.

Writes work/release/brr-dropin-fix-<version>.zip containing our TIMER.DLL and launcher
(Big Red Racing.exe, from work/build/dropin/ - run dropin-build first), BRRFIX.INI and
ddraw.ini (defaults, with comments), README.txt, THIRD-PARTY.txt, LICENSE.txt once the
project has a licence, and cnc-ddraw (ddraw.dll and the three shaders our presets use),
taken from its pinned official release zip, checked by SHA-256 (downloaded into
tools/cnc-ddraw/ if missing), with its licence. THIRD-PARTY.txt (also written to the
repository root) lists every third-party part with its version, source and licence. Text files get Windows line endings. Every file is checked against the game
manifests (data/game-manifest.json): any file whose SHA-256 matches a game file is
refused. Prints the zip's contents. It doesn't publish anything.
Example:
    python3 brr.py dropin-package
"""

import hashlib
import io
import json
import re
import urllib.request
import zipfile

from common import DATA, ROOT, WORK
from prepkit import rel

HELP = "zip the drop-in fix for players (our files only, checked against the game manifest)"

SOURCE = ROOT / "src" / "dropin"
BUILD = WORK / "build" / "dropin"
RELEASE = WORK / "release"
MANIFEST = DATA / "game-manifest.json"
LICENCES = [ROOT / "LICENSE", ROOT / "LICENSE.txt", ROOT / "LICENSE.md"]
THIRD_PARTY = ROOT / "tools" / "third-party"
CNC_DDRAW = {"version": "v7.1.0.0",
             "zip": "https://github.com/FunkyFr3sh/cnc-ddraw/releases/download/v7.1.0.0/cnc-ddraw.zip",
             "zip_sha256": "0b13ab89a64c9918189b1dadd449ef6ed3cb3b7b19cabd96d8adbd95505bb908",
             "licence": "https://raw.githubusercontent.com/FunkyFr3sh/cnc-ddraw/v7.1.0.0/LICENSE",
             "licence_sha256": "a705144535bbfccc05e8310c4f8ab5f0b6f0ec2c9371b76ca5a522cf90e031f5"}
# from the release zip: cnc-ddraw itself, and the shaders our ddraw.ini presets name
CNC_FILES = ["ddraw.dll", "Shaders/nearest-neighbor.glsl", "Shaders/interpolation/lanczos2-sharp.glsl",
             "Shaders/interpolation/catmull-rom-bilinear.glsl"]
# A fixed date for every entry, so the zip is the same each time it's built.
ZIP_DATE = (2026, 1, 1, 0, 0, 0)


def game_checksums():
    manifest = json.loads(MANIFEST.read_text())
    return {entry["sha256"]: f"{tree}/{name}" for tree, files in manifest.items()
            for name, entry in files.items()}


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")


def fetch(url, path, digest):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        with urllib.request.urlopen(url, timeout=120) as response:
            path.write_bytes(response.read())
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise SystemExit(f"dropin-package: {rel(path)} doesn't match its pinned SHA-256; delete it to download again")
    return data


def cnc_ddraw_files():
    release = fetch(CNC_DDRAW["zip"], ROOT / "tools" / "cnc-ddraw" / "cnc-ddraw.zip", CNC_DDRAW["zip_sha256"])
    licence = fetch(CNC_DDRAW["licence"], ROOT / "tools" / "cnc-ddraw" / f"LICENSE-{CNC_DDRAW['version']}",
                    CNC_DDRAW["licence_sha256"])
    with zipfile.ZipFile(io.BytesIO(release)) as archive:
        names = {n.replace("\\", "/"): n for n in archive.namelist()}
        files = {name: archive.read(names[name]) for name in CNC_FILES}
    return files, licence.decode("utf-8")


def licence_block(text, start_marker):
    """The licence text at the end of a single-file library (from start_marker)."""
    i = text.find(start_marker)
    return text[i:].strip().strip("*/").strip() if i >= 0 else "(see the source file)"


def third_party_text(cnc_licence, shaders):
    from dropin_build import DECODERS
    parts = ["Third-party code in the Big Red Racing drop-in fix",
             "===================================================",
             "Everything else in this package is the project's own code. Each part below is",
             "pinned to a release or commit and checked by SHA-256 when the package is built.", ""]
    parts += ["cnc-ddraw " + CNC_DDRAW["version"] + " (ddraw.dll, Shaders\\nearest-neighbor.glsl)",
              "  Source: https://github.com/FunkyFr3sh/cnc-ddraw (official release cnc-ddraw.zip,",
              "  SHA-256 " + CNC_DDRAW["zip_sha256"] + ")", "  Licence:", "",
              cnc_licence.strip(), ""]
    for name, data in shaders.items():
        header = data.decode("latin-1")
        block = header[header.find("/*"):header.find("*/") + 2] if "/*" in header else ""
        parts += [f"{name.replace('/', chr(92))} (from the same cnc-ddraw release)", "  Licence, from the file:", "",
                  block.strip() or "(cnc-ddraw's, above)", ""]
    for folder, filename, url, digest in DECODERS:
        text = (THIRD_PARTY / folder / filename).read_text(errors="replace")
        parts += [f"{filename} ({folder}), compiled into TIMER.DLL", f"  Source: {url}", f"  SHA-256 {digest}",
                  "  Licence, from the file:", "",
                  licence_block(text, "This software is available as a choice of the following licenses")
                  if "This software is available as a choice" in text
                  else licence_block(text, "This software is available under 2 licenses"), ""]
    return "\n".join(parts) + "\n"


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    dll = BUILD / "TIMER.DLL"
    if not dll.exists():
        print("dropin-package: build it first: python3 brr.py dropin-build")
        return 1
    version = re.search(r'BRRFIX_VERSION "([^"]+)"', (SOURCE / "version.h").read_text()).group(1)
    winrace = json.loads(MANIFEST.read_text())["game"]["WINRACE.EXE"]["sha256"]
    readme = (SOURCE / "README.txt").read_text().replace("@VERSION@", version)
    readme = readme.replace("@WINRACE_SHA256@", winrace)
    cnc, cnc_licence = cnc_ddraw_files()
    shaders = {name: data for name, data in cnc.items() if name.startswith("Shaders/")}
    third_party = third_party_text(cnc_licence, {k: v for k, v in shaders.items() if "interpolation" in k})
    (ROOT / "THIRD-PARTY.txt").write_text(third_party)
    files = {"Big Red Racing.exe": (BUILD / "Big Red Racing.exe").read_bytes(),
             "TIMER.DLL": dll.read_bytes(),
             "BRRFIX.INI": crlf((SOURCE / "BRRFIX.INI").read_text()),
             "ddraw.ini": crlf((SOURCE / "ddraw.ini").read_text()),
             "ddraw.dll": cnc["ddraw.dll"],
             **shaders,
             "README.txt": crlf(readme),
             "THIRD-PARTY.txt": crlf(third_party)}
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
        print(f"    {name:48} {len(data):>9,} bytes  sha256 {hashlib.sha256(data).hexdigest()[:16]}...")
    print(f"  checked against {len(known):,} game files: no matches"
          + ("" if licence else "; no LICENSE yet (the project's licence isn't chosen)"))
    print("dropin-package: PASS")
    return 0
