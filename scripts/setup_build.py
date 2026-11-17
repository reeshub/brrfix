r"""setup-build: make "Install Game Files.bat", the players' installer for the game files.

Fills the template src/setup/install-game-files.bat with facts from data/: every file of
the patched game (data/game-manifest.json: its name, where it comes from, its size and
SHA-256), the installed folders and the three files the 1996 installer writes
(data/install-layout.json, made again with prep_install), the zips' names and checksums
(data/inputs.json) and the CD's audio tracks (data/disc.json), plus the pinned music
converters below. Writes work/build/dropin/Install Game Files.bat (ASCII, Windows line
endings); dropin-package puts it in the zip next to Big Red Racing.exe. Needs work/ from
python3 brr.py prepare (for the installer's files).

The batch file runs Windows PowerShell 5.1 (part of Windows 10 and 11) on the script after
its marker line. It reads the disc image straight out of its zip in one pass (the data
track, then the audio tracks), installs and patches the game into Game\ as the prepare
pipeline does (checked file by file), converts the music into Music\ (FLAC, Ogg Vorbis or
WAV; the converter is downloaded into Tools\ and checked by SHA-256), and offers a desktop
shortcut and to delete the zips and the converter. Test it with python3 brr.py setup-test.
Example:
    python3 brr.py setup-build
"""

import base64
import hashlib
import json
import re

from common import DATA, ROOT, WORK
from prepkit import rel

HELP = 'make "Install Game Files.bat", the players\' installer for the game files (from data/)'

TEMPLATE = ROOT / "src" / "setup" / "install-game-files.bat"
OUT_NAME = "Install Game Files.bat"
OUT = WORK / "build" / "dropin" / OUT_NAME
MARKER = "##BRR-SETUP-POWERSHELL##"

# The music converters, pinned (official sources; KNOWLEDGE "Tools"). FLAC: Xiph.Org's own
# Windows build. FFmpeg (for Ogg Vorbis; essentials has libvorbis): Gyan Doshi's release
# build, one of the two Windows builds ffmpeg.org links to, from its GitHub releases.
FLAC_URL = "https://ftp.osuosl.org/pub/xiph/releases/flac/flac-1.5.0-win.zip"
FLAC = {"Label": "FLAC 1.5.0", "Source": "the official FLAC release from Xiph.Org", "Url": FLAC_URL,
        "Zip": "flac-1.5.0-win.zip",
        "ZipSha256": "53f1500f0d6e7c61379d7fee50d4a9f7f504c650009506d9ba015530d76c0dde", "ZipSize": 1318000,
        "Folder": "flac", "Exe": "flac/flac.exe"}
FLAC_FILES = {
    "Win64": [("flac.exe", "ff23d9cbc11d18c02f262c3ee455830ea13fbe8d9876249f0bdf101e3ad66709"),
              ("libFLAC.dll", "f93499172875fc2c0df80b57086f32e3f39e835283952ee2a59a3d4ffb097644")],
    "Win32": [("flac.exe", "713e5d5d9d8b3222cc5459e4770fc86df0f64cbaebdbfd6430f1996bd0aa6f05"),
              ("libFLAC.dll", "617bc691e83e82522284626501bf8564aa34abd97af7763f13dc8c21bd3fd205")],
}
FFMPEG = {"Label": "FFmpeg 9.0.2", "Source": "a build linked from ffmpeg.org (gyan.dev, on GitHub)",
          "Url": "https://github.com/GyanD/codexffmpeg/releases/download/9.0.2/ffmpeg-9.0.2-essentials_build.zip",
          "Zip": "ffmpeg-9.0.2-essentials_build.zip",
          "ZipSha256": "60f467265b1e312373dbcd92200c2618a74850f98d3d078e94296bb3fa2047ba", "ZipSize": 114768076,
          "Folder": "ffmpeg", "Exe": "ffmpeg/ffmpeg.exe",
          "Files": [{"Entry": "ffmpeg-9.0.2-essentials_build/bin/ffmpeg.exe", "Target": "ffmpeg/ffmpeg.exe",
                     "Sha256": "3256173f3f8bffd7df12227c68adf68025edb1832273a9530688a7bb1ed8edec"}]}
ZIP_WHAT = {"disc": "the CD as a disc image", "patch": "the official v2.25 patch"}


def ps_string(value):
    return "'" + str(value).replace("'", "''") + "'"


def ps_value(value):
    if isinstance(value, bool):
        return "$true" if value else "$false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, dict):
        return "@{ " + "; ".join(f"{key} = {ps_value(item)}" for key, item in value.items()) + " }"
    if isinstance(value, list):
        return "@(" + ", ".join(ps_value(item) for item in value) + ")"
    return ps_string(value)


def flac_tool(arch):
    files = [{"Entry": f"flac-1.5.0-win/{arch}/{name}", "Target": f"flac/{name}", "Sha256": digest}
             for name, digest in FLAC_FILES[arch]]
    return dict(FLAC, Label=FLAC["Label"] + (" (32-bit)" if arch == "Win32" else ""), Files=files)


def data_block(version):
    """The PowerShell assignments that replace #@DATA@"""
    import prep_install
    from prep_unpack import ZIPS
    manifest = json.loads((DATA / "game-manifest.json").read_text())["game"]
    layout = json.loads((DATA / "install-layout.json").read_text())
    inputs = json.loads((DATA / "inputs.json").read_text())
    disc = json.loads((DATA / "disc.json").read_text())

    installer = prep_install.generated_files()
    for name, content in installer.items():
        if manifest.get(name, {}).get("sha256") != hashlib.sha256(content).hexdigest():
            raise SystemExit(f"setup-build: the installer's {name} doesn't match data/game-manifest.json")
    lines = []
    for name, entry in sorted(manifest.items()):
        if "|" in name or "|" in entry["origin"]:
            raise SystemExit(f"setup-build: can't list {name}")
        lines.append(f"{name}|{entry['origin']}|{entry['size']}|{entry['sha256']}")
    audio = [t for t in disc["tracks"] if t["type"] == "audio"]
    zips = {}
    for key, name in ZIPS.items():
        stem = name[:-len(".zip")]
        zips[key] = {"Name": name, "Pattern": stem + "*.zip", "Sha256": inputs["zips"][name]["sha256"],
                     "Size": inputs["zips"][name]["size"], "What": ZIP_WHAT[key]}
    out = [f"$SetupVersion = {ps_string(version)}",
           f"$DiscZipInfo = {ps_value(zips['disc'])}",
           f"$PatchZipInfo = {ps_value(zips['patch'])}",
           f"$GameFolders = {ps_value(sorted(layout['folders']))}",
           "$GameFileList = @'", *lines, "'@",
           "$InstallerFiles = @{"]
    out += [f"    {ps_string(name)} = {ps_string(base64.b64encode(content).decode('ascii'))}"
            for name, content in sorted(installer.items())]
    stamp = prep_install.INSTALL_TIME
    out += ["}",
            f"$InstallerTime = New-Object DateTime ({stamp.year}, {stamp.month}, {stamp.day}, "
            f"{stamp.hour}, {stamp.minute}, {stamp.second})",
            f"$AudioTrackCount = {len(audio)}",
            "$AudioTracks = @{"]
    out += [f"    {t['number']} = @{{ Start = {t['start']}; Sectors = {t['sectors']}; "
            f"Sha256 = {ps_string(t['audio_sha256'])} }}" for t in audio]
    out += ["}",
            f"$FlacTool = {ps_value(flac_tool('Win64'))}",
            f"$FlacTool32 = {ps_value(flac_tool('Win32'))}",
            f"$FfmpegTool = {ps_value(FFMPEG)}"]
    return "\n".join(out), len(lines), len(audio)


def make():
    """The batch file's bytes (ASCII, CRLF), and a one-line summary."""
    version = re.search(r'BRRFIX_VERSION "([^"]+)"', (ROOT / "src" / "dropin" / "version.h").read_text()).group(1)
    template = TEMPLATE.read_text()
    if template.count("#@DATA@") != 1 or template.count(MARKER) != 1:
        raise SystemExit(f"setup-build: {rel(TEMPLATE)} should have one #@DATA@ line and one {MARKER} line")
    block, files, tracks = data_block(version)
    text = template.replace("#@DATA@", block)
    if text.count(MARKER) != 1:
        raise SystemExit(f"setup-build: {MARKER} would appear more than once")
    data = text.replace("\r\n", "\n").replace("\n", "\r\n").encode("ascii")
    return data, f"{files} game files, {tracks} music tracks, version {version}"


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    data, summary = make()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(data)
    print(f"  {rel(OUT)} ({len(data):,} bytes): {summary}")
    print("setup-build: PASS")
    return 0
