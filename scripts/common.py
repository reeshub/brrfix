# brrfix's own small version of the development repository's common.py: only what the build
# scripts here use. Written by that repository's brrfix-sync; don't edit it here.
"""This repository's folders, for the build scripts."""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
WORK = ROOT / "work"
TOOLS = ROOT / "tools"
