#!/usr/bin/env python3
# Written by the development repository's brrfix-sync, like the stand-ins in scripts/; don't
# edit it here.
"""Build Big Red Launcher and BRRPATCH from source, and zip them as they are released.

    python3 build.py --get-watcom    download Open Watcom v2 (the 2026-11-01 snapshot, checked
                                     by SHA-256) into tools/open-watcom/
    python3 build.py GAME_FOLDER     build both into work/build/, and the two zips into
                                     work/release/

GAME_FOLDER is your own copy of Big Red Racing, patched to v2.25: the Game folder that
"Install Game Files.bat" makes, for example. The builds check their work against its
WINRACE.EXE and RACING.EXE and take the launcher's icon from WINRACE.EXE; nothing of the
game goes into what they make (the packaging checks every file). They also download three
open-source parts, pinned and checked by SHA-256: dr_flac and stb_vorbis (compiled into
PATCH.DLL) and cnc-ddraw (in the launcher's zip). See BUILDING.md.
"""

import hashlib
import importlib
import json
import os
import sys
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "scripts"))

WATCOM_URL = "https://github.com/open-watcom/open-watcom-v2/releases/download/2026-11-01-Build/ow-snapshot.tar.xz"
WATCOM_SHA256 = "89e90b7544cf3ea4a38124ec4c104a5027f31101405d7115c3f833daa465bb48"
WATCOM = ROOT / "tools" / "open-watcom" / "2026-11-01"
GAME = ROOT / "work" / "game"
CHECKED = ["WINRACE.EXE", "RACING.EXE", "TIMER.DLL"]
STEPS = [("dropin_build", "Big Red Launcher: PATCH.DLL, DPLAY.DLL and Big Red Racing.exe"),
         ("brrpatch_build", "BRRPATCH.EXE, the DOS patcher"),
         ("dropin_package", "the Big Red Launcher zip"),
         ("dos_package", "the DOS patch zip")]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while chunk := handle.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def get_watcom():
    if (WATCOM / "binl64" / "wcc386").exists():
        print(f"Open Watcom is already in {WATCOM.relative_to(ROOT)}/")
        return 0
    archive = WATCOM.parent / "ow-snapshot-2026-11-01.tar.xz"
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or sha256(archive) != WATCOM_SHA256:
        print(f"Downloading {WATCOM_URL} (150 MB) ...")
        urllib.request.urlretrieve(WATCOM_URL, archive)
    if sha256(archive) != WATCOM_SHA256:
        archive.unlink()
        print("build: the download isn't the expected file (its SHA-256 differs), so it was deleted")
        return 1
    print(f"Unpacking it into {WATCOM.relative_to(ROOT)}/ ...")
    with tarfile.open(archive) as tar:
        if hasattr(tarfile, "data_filter"):
            tar.extractall(WATCOM, filter="data")
        else:
            tar.extractall(WATCOM)
    archive.unlink()
    print("Open Watcom: ready")
    return 0


def use_game(folder):
    """work/game: a link to the player's game, once its main files are checked as v2.25's"""
    folder = Path(folder).expanduser().resolve()
    manifest = json.loads((ROOT / "data" / "game-manifest.json").read_text())["game"]
    for name in CHECKED:
        path = folder / name
        if not path.is_file():
            print(f"build: {path} is missing: GAME_FOLDER should be Big Red Racing patched to v2.25")
            return False
        if sha256(path) != manifest[name]["sha256"]:
            print(f"build: {path} isn't the v2.25 game's {name}"
                  + (" (patched by the DOS patch? Use a fresh Game folder)" if name == "RACING.EXE" else ""))
            return False
    GAME.parent.mkdir(parents=True, exist_ok=True)
    if GAME.is_symlink():
        GAME.unlink()
    elif GAME.exists():
        print(f"build: {GAME.relative_to(ROOT)} is in the way (it should be a link); remove it first")
        return False
    os.symlink(folder, GAME, target_is_directory=True)
    print(f"Game: {folder} (its WINRACE.EXE, RACING.EXE and TIMER.DLL are v2.25's)")
    return True


def main(argv):
    if sys.version_info < (3, 10):
        print("build: needs Python 3.10 or later")
        return 1
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0 if argv else 1
    if argv[0] == "--get-watcom":
        return get_watcom()
    if not (WATCOM / "binl64" / "wcc386").exists():
        print("build: Open Watcom isn't there yet: python3 build.py --get-watcom")
        return 1
    if not use_game(argv[0]):
        return 1
    for module, what in STEPS:
        print(f"\n== {what}")
        code = importlib.import_module(module).main([])
        if code:
            print(f"build: {module} failed")
            return code
    print("\nbuild: done; the two zips are in work/release/")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
