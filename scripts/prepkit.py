# brrfix's own small version of the development repository's prepkit.py: only what the build
# scripts here use. Written by that repository's brrfix-sync; don't edit it here.
"""Paths in messages."""

from pathlib import Path

from common import ROOT


def rel(path):
    """A path relative to this repository, with forward slashes, for messages."""
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)
