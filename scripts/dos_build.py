# brrfix's own small version of the development repository's dos_build.py: only what the build
# scripts here use. Written by that repository's brrfix-sync; don't edit it here.
"""The DOS patch: its source, and the game file it patches.

src/dospatch/ is the DOS patch's source. data/dos-patches.json is its compiled form (our
code, where it goes in RACING.EXE, and checksums), which BRRPATCH, Big Red Launcher's "Patch
the DOS version" button and the Python patcher apply; compiling it again takes the
development repository's tools, which this repository doesn't have.
"""

from common import ROOT, WORK

SOURCE = ROOT / "src" / "dospatch"
GAME = WORK / "game" / "RACING.EXE"
EXPORTS = WORK / "not-in-this-repository"


def main(args):
    print("dos-build: data/dos-patches.json, the DOS patch's compiled form, is made in the development repository")
    return 0
