"""dropin-build: build Big Red Launcher (PATCH.DLL and the launcher) into work/build/dropin/.

PATCH.DLL is the fix: it stands in for the game's TIMER.DLL (the same exports) and patches
the game in memory. The launcher makes WINRACEP.EXE, its own copy of the player's
WINRACE.EXE, with its section headers corrected (win_headers.py) and its import of
timer.dll changed to PATCH.DLL (the same length), so the game loads ours and the game's
own TIMER.DLL stays as it is; and, when cnc-ddraw is there (shipped as CNCDD.DLL), its
import of DDRAW.dll changed to CNCDD.DLL, so Wine and Proton can't put their own
DirectDraw in its place (winracep() below; src/launcher/winfix.c does the same).
1. Checks work/game/WINRACE.EXE is the v2.25 file from the game manifest.
2. Checks every patch site and protected range in data/dropin-patches.json against it
   by SHA-256 (recording the values the first time), and writes sites.h from them, with
   the checksums of WINRACEP.EXE (with and without cnc-ddraw), and of WINRACE.EXE as
   installed and header-corrected in place (as launchers before 2026-11-15 did): the DLL
   accepts all four.
   Writes splashw.h from src/dropin/BRS2.RAW-wide.png: the widescreen version of the start-up
   splash (BRS2.RAW) made for Big Red Launcher, read here without Pillow.
3. Compiles src/dropin/ with Open Watcom v2 (the pinned snapshot in tools/open-watcom/,
   see docs/KNOWLEDGE.md, Tools; Linux programs in binl64/, or macOS ones built from
   source in armo64/ or bino64/) for Windows 95 and later: 486 instructions, its own C
   runtime linked in, so the DLL needs only Windows system DLLs (not msvcrt.dll).
4. Builds the launcher from src/launcher/ with the same compiler: the header corrector
   (winfix.c), and the "Patch the DOS version" button, which compiles in the DOS patcher
   (src/brrpatch/brrpatch.c) with the DOS patch's steps from brrpatch-build's brrdata.h,
   made here from data/dos-patches.json and work/game/RACING.EXE (so run dos-build first
   if it changed that file). brrpatch.c and winfix.c use standard C file functions (for
   DOS and for tests on any machine); here they get Windows' own instead
   (src/launcher/winfile.h, forced in), because Open Watcom's fread brings console
   functions Windows 95 doesn't have.
5. Checks both programs: imports from Windows system DLLs only, every function in
   data/win95-api.json (Windows 95's; dropin-compat --dlls checks against real ones),
   and PE headers marked for Windows 4.0; and the DLL has exactly the original
   TIMER.DLL's exports and ordinals. Copies BRRFIX.INI and ddraw.ini beside them.
   WINRACEP.EXE itself is game content: only the launcher makes it, from the player's file.
Example:
    python3 brr.py dropin-build
"""

import hashlib
import json
import os
import platform
import re
import shutil
import struct
import subprocess
import textwrap
import urllib.request
import zlib

from common import DATA, ROOT, WORK
from prepkit import rel

HELP = "build Big Red Launcher (PATCH.DLL and the launcher) into work/build/dropin/"

SOURCE = ROOT / "src" / "dropin"
OUT = WORK / "build" / "dropin"
PATCHES = DATA / "dropin-patches.json"
WIN95_API = DATA / "win95-api.json"
MANIFEST = DATA / "game-manifest.json"
GAME = WORK / "game"
# The widescreen start-up splash made for Big Red Launcher: 320x180, BRS2.RAW's palette
SPLASH_WIDE = ROOT / "src" / "dropin" / "BRS2.RAW-wide.png"
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "winmm.dll", "advapi32.dll"}
C_FILES = ["main.c", "clock.c", "patch.c", "interp.c", "camera.c", "debug.c", "angles.c", "display.c",
           "music.c", "decoders.c", "sha256.c", "tracks.c", "controller.c", "keys.c", "render.c", "distance.c",
           "sky.c", "shade.c", "palanim.c", "hud.c", "persp.c", "horizon.c", "sound.c", "menunav.c", "navwin.c",
           "splash.c", "pause.c"]
# Third-party code compiled in: pinned, downloaded at build time, checked by SHA-256, never committed
THIRD_PARTY = ROOT / "tools" / "third-party"
DECODERS = [
    ("dr_flac-flac-0.13.4", "dr_flac.h",
     "https://raw.githubusercontent.com/mackron/dr_libs/flac-0.13.4/dr_flac.h",
     "b8078eeb5d523502eb7964508cd7ec8450bc79d383bec90aa17fd9e2fefd9c24"),
    ("stb_vorbis-4e9b368", "stb_vorbis.c",
     "https://raw.githubusercontent.com/nothings/stb/4e9b368bfc071e97206ea5c6adba68dd0cbf8897/stb_vorbis.c",
     "4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636"),
]
# their code isn't ours to tidy: compiled with warnings off
QUIET_FILES = {"decoders.c"}
ASM_FILES = ["thunks.asm"]
# The original TIMER.DLL's exports: (ordinal, name, our symbol)
EXPORTS = [(1, "_TIMER_Count", "_brr_timer_count"), (2, "_TIMER_Handler@20", "_brr_TIMER_Handler"),
           (3, "TIMER_Init_", "_brr_TIMER_Init"), (4, "TIMER_Shutdown_", "_brr_TIMER_Shutdown")]
# -ecc: C calling convention by default (the thunks call our C that way); -4 -fp3: 486
# instructions and inline 387 maths; -zp8 -ei: structure layout and enums as MSVC/GCC do them
CFLAGS = ["-q", "-bt=nt", "-bd", "-za99", "-ecc", "-4", "-fp3", "-zp8", "-ei", "-s", "-ox",
          "-w4", "-we"]
WATCOM = ROOT / "tools" / "open-watcom" / "2026-11-01"
WATCOM_RELEASE = "https://github.com/open-watcom/open-watcom-v2/releases/tag/2026-11-01-Build"


class BuildError(Exception):
    pass


# ---- a minimal PE reader (sections, exports, imports) ----
class PE:
    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise BuildError("not a PE file")
        sections, opt_size = struct.unpack_from("<H12xH", data, pe + 6)
        opt = pe + 24
        self.image_base = struct.unpack_from("<I", data, opt + 28)[0]
        self.dirs = [struct.unpack_from("<II", data, opt + 96 + 8 * i) for i in range(16)]
        self.sections = []
        for i in range(sections):
            s = opt + opt_size + 40 * i
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, s + 8)
            self.sections.append((va, max(vsize, rsize), raw, rsize))

    def offset(self, rva):
        for va, size, raw, rsize in self.sections:
            if va <= rva < va + size:
                if rva - va >= rsize:
                    raise BuildError(f"RVA 0x{rva:X} is in uninitialised data")
                return raw + rva - va
        raise BuildError(f"RVA 0x{rva:X} is not in any section")

    def read(self, address, size):
        start = self.offset(address - self.image_base)
        return self.data[start:start + size]

    def cstring(self, rva):
        start = self.offset(rva)
        return self.data[start:self.data.index(b"\0", start)].decode("latin-1")

    def exports(self):
        rva, size = self.dirs[0]
        if not size:
            return []
        d = self.offset(rva)
        base, nfunc, nnames, funcs, names, ords = struct.unpack_from("<16xIIIIII", self.data, d)
        named = {}
        for i in range(nnames):
            name = self.cstring(struct.unpack_from("<I", self.data, self.offset(names) + 4 * i)[0])
            named[struct.unpack_from("<H", self.data, self.offset(ords) + 2 * i)[0]] = name
        return sorted((base + i, named.get(i, "")) for i in range(nfunc))

    def header(self):
        """(machine, OS version, subsystem, subsystem version) from the PE headers."""
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        machine = struct.unpack_from("<H", self.data, pe + 4)[0]
        opt = pe + 24
        os_major, os_minor = struct.unpack_from("<HH", self.data, opt + 40)
        sub_major, sub_minor = struct.unpack_from("<HH", self.data, opt + 48)
        subsystem = struct.unpack_from("<H", self.data, opt + 68)[0]
        return machine, (os_major, os_minor), subsystem, (sub_major, sub_minor)

    def imported_functions(self):
        """{dll: [function names, or #ordinal]} from the import table."""
        rva, size = self.dirs[1]
        found, d = {}, self.offset(rva) if size else None
        while d is not None:
            lookup, _t, _f, name_rva, thunk = struct.unpack_from("<IIIII", self.data, d)
            if not name_rva:
                break
            names, entry = [], self.offset(lookup or thunk)
            while True:
                value = struct.unpack_from("<I", self.data, entry)[0]
                if not value:
                    break
                names.append(f"#{value & 0xFFFF}" if value & 0x80000000
                             else self.cstring(value + 2))
                entry += 4
            found[self.cstring(name_rva)] = names
            d += 20
        return found

    def import_name_offsets(self):
        """{dll name: file offset of the name} from the import table."""
        rva, size = self.dirs[1]
        found, d = {}, self.offset(rva) if size else None
        while d is not None:
            name_rva = struct.unpack_from("<I", self.data, d + 12)[0]
            if not name_rva:
                break
            found[self.cstring(name_rva)] = self.offset(name_rva)
            d += 20
        return found

    def imports(self):
        rva, size = self.dirs[1]
        found, d = [], self.offset(rva) if size else None
        while d is not None:
            name_rva = struct.unpack_from("<I", self.data, d + 12)[0]
            if not name_rva:
                break
            found.append(self.cstring(name_rva))
            d += 20
        return found


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def corrected_sha256():
    """The v2.25 WINRACE.EXE's SHA-256 with its section headers corrected in place, as
    launchers before 2026-11-15 left it (still recognised)."""
    import win_headers
    return sha256(win_headers.corrected((GAME / "WINRACE.EXE").read_bytes())[0])


# WINRACEP.EXE's import renames: (the game's import name, ours), each the same length
TIMER_IMPORT = (b"timer.dll", b"PATCH.DLL")
DDRAW_IMPORT = (b"DDRAW.dll", b"CNCDD.DLL")
CNC_DDRAW_DLL = DDRAW_IMPORT[1].decode()


def import_offset(data, name):
    """The file offset of an import's DLL name in the game, checked to be exactly that name."""
    offsets = {n.lower(): at for n, at in PE(data).import_name_offsets().items()}
    at = offsets.get(name.decode().lower())
    if at is None or data[at:at + len(name) + 1] != name + b"\0":
        raise BuildError(f"WINRACE.EXE doesn't import {name.decode()} under exactly that name")
    return at


def winracep(data, cnc_ddraw=True):
    """WINRACEP.EXE from a WINRACE.EXE: section headers corrected, timer.dll imported as
    PATCH.DLL and, with cnc-ddraw, DDRAW.dll as CNCDD.DLL. The launcher's winfix.c does the
    same for the v2.25 file."""
    import win_headers
    fixed = bytearray(win_headers.corrected(data)[0])
    for old, new in (TIMER_IMPORT, DDRAW_IMPORT) if cnc_ddraw else (TIMER_IMPORT,):
        at = import_offset(data, old)
        fixed[at:at + len(new)] = new
    return bytes(fixed)


def winracep_sha256(cnc_ddraw=True):
    return sha256(winracep((GAME / "WINRACE.EXE").read_bytes(), cnc_ddraw))


def check_sites(exe):
    """Check (or record) each site's and range's SHA-256; return the patches document."""
    doc = json.loads(PATCHES.read_text())
    recorded = 0
    for kind, entries in (("site", doc["sites"]), ("protected", doc["protected"])):
        for entry in entries:
            size = entry.get("length", entry.get("size"))
            digest = sha256(exe.read(int(entry["address"], 16), size))
            if "sha256" not in entry:
                entry["sha256"] = digest
                recorded += 1
            elif entry["sha256"] != digest:
                raise BuildError(f"{kind} {entry['name']} at {entry['address']} doesn't match "
                                 f"data/dropin-patches.json")
    if recorded:
        PATCHES.write_text(json.dumps(doc, indent=1) + "\n")
        print(f"  recorded {recorded} checksum(s) in {rel(PATCHES)}")
    return doc


def write_header(doc, exe_sha, exe_size):
    lines = ["/* Generated by python3 brr.py dropin-build from data/dropin-patches.json. */",
             "struct site_info { const char *name, *fix; uint32_t address, length; const char *sha256; };",
             "struct range_info { const char *name; uint32_t address, size; const char *sha256; };",
             f'#define EXPECTED_EXE_SHA256 "{exe_sha}"',
             f'#define EXPECTED_EXE_CORRECTED_SHA256 "{corrected_sha256()}"',
             f'#define EXPECTED_WINRACEP_SHA256 "{winracep_sha256()}"',
             f'#define EXPECTED_WINRACEP_PLAIN_SHA256 "{winracep_sha256(False)}"',
             f'#define CNC_DDRAW_DLL "{CNC_DDRAW_DLL}"',
             f"#define EXPECTED_EXE_SIZE {exe_size}u",
             "static const struct site_info SITES[] = {"]
    for s in doc["sites"]:
        lines.append(f'    {{"{s["name"]}", "{s["fix"]}", {s["address"]}u, {s["length"]}u, "{s["sha256"]}"}},')
    lines += ["};", "static const struct range_info PROTECTED[] = {"]
    for r in doc["protected"]:
        lines.append(f'    {{"{r["name"]}", {r["address"]}u, {r["size"]}u, "{r["sha256"]}"}},')
    lines.append("};")
    (OUT / "sites.h").write_text("\n".join(lines) + "\n")


def png_indexed(path):
    """An 8-bit palette PNG, not interlaced: (width, height, palette bytes, rows of pixels)."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise BuildError(f"{rel(path)} isn't a PNG")
    pos, header, palette, idat = 8, None, b"", []
    while pos + 8 <= len(data):
        size, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + size]
        pos += 12 + size
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = body
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if not header or header[2] != 8 or header[3] != 3 or header[6] != 0:
        raise BuildError(f"{rel(path)} must be an 8-bit palette PNG, not interlaced")
    width, height = header[0], header[1]
    raw = zlib.decompress(b"".join(idat))
    rows, prior = [], bytearray(width)
    for y in range(height):
        line = raw[y * (width + 1):(y + 1) * (width + 1)]
        kind, row = line[0], bytearray(line[1:])
        for x in range(width):
            a = row[x - 1] if x else 0
            b = prior[x]
            c = prior[x - 1] if x else 0
            if kind == 1:
                row[x] = (row[x] + a) & 255
            elif kind == 2:
                row[x] = (row[x] + b) & 255
            elif kind == 3:
                row[x] = (row[x] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(row))
        prior = row
    return width, height, palette, rows


def write_splash():
    """splashw.h: the widescreen splash's pixels, and its palette in the game's 6 bits a channel
    (the DLL shows it only when the game's BRS2.RAW palette is the same)."""
    width, height, palette, rows = png_indexed(SPLASH_WIDE)
    if (width, height) != (320, 180) or len(palette) != 768:
        raise BuildError(f"{rel(SPLASH_WIDE)} must be 320x180 with a 256-colour palette (BRS2.RAW's)")
    def table(values):
        return "\n".join("    " + " ".join(f"{v}," for v in values[i:i + 24]) for i in range(0, len(values), 24))
    pixels = b"".join(rows)
    (OUT / "splashw.h").write_text(
        f"/* Generated by python3 brr.py dropin-build from {SPLASH_WIDE.relative_to(ROOT).as_posix()}: the\n"
        " * widescreen version of the start-up splash (BRS2.RAW) made for Big Red Launcher, and its\n"
        " * palette (6 bits a channel). render.c only. */\n"
        f"#define SPLASH_WIDE_W {width}\n#define SPLASH_WIDE_H {height}\n"
        "static const unsigned char splash_wide_palette[768] = {\n" + table([v >> 2 for v in palette]) + "\n};\n"
        f"static const unsigned char splash_wide[{width * height}] = {{\n" + table(pixels) + "\n};\n")


def watcom_bin():
    """Open Watcom's programs: Linux (the release snapshot), or macOS ARM or Intel (built from source).
    Chosen by this machine, not by what's there: a copied tools/ folder can hold another's."""
    if platform.system() == "Darwin":
        names = ("armo64", "bino64") if platform.machine() == "arm64" else ("bino64",)
    else:
        names = ("binl64",)
    for name in names:
        if (WATCOM / name / "wcc386").exists():
            return WATCOM / name
    return WATCOM / names[0]


def watcom_env():
    if not (watcom_bin() / "wcc386").exists():
        raise BuildError(f"Open Watcom isn't in {rel(WATCOM)}: unpack ow-snapshot.tar.xz from "
                         f"{WATCOM_RELEASE} there (SHA-256 in docs/KNOWLEDGE.md, Tools)")
    return dict(os.environ, WATCOM=str(WATCOM), INCLUDE=f"{WATCOM / 'h'}:{WATCOM / 'h' / 'nt'}",
                PATH=f"{watcom_bin()}:{os.environ.get('PATH', '')}")


def run_tool(command, env, what, cwd=None):
    result = subprocess.run(command, capture_output=True, text=True, env=env, cwd=cwd)
    output = (result.stdout + result.stderr).strip()
    if result.returncode != 0:
        raise BuildError(f"{what} failed:\n" + output[-4000:])


def fetch_decoders():
    """Download the pinned decoders if missing, and check every byte."""
    folders = []
    for folder, name, url, digest in DECODERS:
        path = THIRD_PARTY / folder / name
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(url, timeout=60) as response:
                path.write_bytes(response.read())
            print(f"  downloaded {rel(path)}")
        if sha256(path.read_bytes()) != digest:
            raise BuildError(f"{rel(path)} doesn't match its pinned SHA-256; delete it to download it again")
        folders.append(path.parent)
    return folders


def compile_dll(dll):
    env = watcom_env()
    # The decoders' folders relative to the repository, as the compiler runs there: their
    # assert messages record each file's path as it was found, which would otherwise put the
    # builder's own folder (user name included) into PATCH.DLL
    includes = [f"-i={folder.relative_to(ROOT).as_posix()}" for folder in fetch_decoders()]
    objects = OUT / "obj"
    objects.mkdir(exist_ok=True)
    files = []
    for name in C_FILES:
        obj = objects / (name.rsplit(".", 1)[0] + ".obj")
        flags = [f for f in CFLAGS if f not in ("-w4", "-we")] + ["-w0"] if name in QUIET_FILES else CFLAGS
        run_tool([str(watcom_bin() / "wcc386"), *flags, f"-i={OUT}", f"-i={SOURCE}", *includes,
                  f"-fo={obj}", str(SOURCE / name)], env, f"compiling {name}", cwd=ROOT)
        files.append(obj)
    for name in ASM_FILES:
        obj = objects / (name.rsplit(".", 1)[0] + ".obj")
        run_tool([str(watcom_bin() / "wasm"), "-q", "-4", f"-fo={obj}", str(SOURCE / name)],
                 env, f"assembling {name}")
        files.append(obj)
    script = OUT / "patch.lnk"
    script.write_text("\n".join([
        "system nt_dll initinstance terminstance",
        f"name '{dll}'",
        f"option quiet, caseexact, map='{OUT / 'PATCH.map'}'",
        "option osversion=4.0",
        "runtime windows=4.0",
        "library winmm.lib",
        *[f"file '{f}'" for f in files],
        *[f"export '{name}'.{ordinal}={symbol}" for ordinal, name, symbol in EXPORTS],
    ]) + "\n")
    run_tool([str(watcom_bin() / "wlink"), f"@{script}"], env, "linking")


LAUNCHER_SOURCE = ROOT / "src" / "launcher"
LAUNCHER = "Big Red Racing.exe"
BRRPATCH_SOURCE = ROOT / "src" / "brrpatch"
LAUNCHER_FILES = [LAUNCHER_SOURCE / "launcher.c", LAUNCHER_SOURCE / "winfix.c", LAUNCHER_SOURCE / "winfile.c",
                  SOURCE / "sha256.c", SOURCE / "tracks.c", BRRPATCH_SOURCE / "brrpatch.c"]
# headers forced into a source file: the file patchers get Windows' file functions (winfile.h)
LAUNCHER_FORCED = {"brrpatch.c": LAUNCHER_SOURCE / "winfile.h", "winfix.c": LAUNCHER_SOURCE / "winfile.h"}
LAUNCHER_DLLS = SYSTEM_DLLS | {"shell32.dll", "comctl32.dll"}
HELP_SOURCE = LAUNCHER_SOURCE / "HELP.md"
HELP_FILE = "Big Red Launcher Help.txt"
RT_ICON, RT_GROUP_ICON = 3, 14


def resources(pe, wanted):
    """{name or id: data} for one resource type, first language of each."""
    rva, size = pe.dirs[2]
    if not size:
        return {}
    base = pe.offset(rva)

    def entries(at):
        named, ids = struct.unpack_from("<12xHH", pe.data, at)
        return [struct.unpack_from("<II", pe.data, at + 16 + 8 * i) for i in range(named + ids)]

    found = {}
    for kind, below in entries(base):
        if kind != wanted:
            continue
        for name, langs in entries(base + (below & 0x7FFFFFFF)):
            _, leaf = entries(base + (langs & 0x7FFFFFFF))[0]
            data_rva, data_size = struct.unpack_from("<II", pe.data, base + leaf)
            start = pe.offset(data_rva)
            found[name] = pe.data[start:start + data_size]
    return found


def extract_icon(exe, path):
    """The game's own icon (its first icon group) from WINRACE.EXE, as an .ico file
    (the director confirmed it may be used, 2026-11-09)."""
    pe = PE(exe)
    groups = resources(pe, RT_GROUP_ICON)
    icons = resources(pe, RT_ICON)
    if not groups:
        raise BuildError("WINRACE.EXE has no icon")
    group = next(iter(groups.values()))
    count = struct.unpack_from("<H", group, 4)[0]
    entries, images = [], []
    for i in range(count):
        fields = struct.unpack_from("<BBBBHHIH", group, 6 + 14 * i)
        images.append(icons[fields[7]])
        entries.append(fields[:7])
    out = struct.pack("<HHH", 0, 1, count)
    offset = 6 + 16 * count
    for fields, image in zip(entries, images):
        out += struct.pack("<BBBBHHII", *fields[:6], len(image), offset)
        offset += len(image)
    path.write_bytes(out + b"".join(images))
    return count


def inline_text(text):
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    text = re.sub(r"(?<![\w*])\*([^*]+)\*(?![\w*])", r"\1", text)
    return text.replace("`", "")


def markdown_to_text(markdown, width=78):
    """Plain text for Notepad from our simple Markdown: headings underlined, paragraphs and
    list items wrapped, code blocks indented, links written out."""
    out, paragraph, indent, code = [], [], "", False

    def flush():
        nonlocal paragraph, indent
        if paragraph:
            first, rest = (indent, " " * len(indent)) if indent else ("", "")
            out.extend(textwrap.wrap(inline_text(" ".join(paragraph)), width, initial_indent=first,
                                     subsequent_indent=rest, break_on_hyphens=False))
        paragraph, indent = [], ""

    for line in markdown.splitlines():
        stripped = line.strip()
        if stripped.startswith("```"):
            flush()
            code = not code
            continue
        if code:
            out.append("    " + line)
            continue
        heading = re.match(r"(#{1,3}) (.*)", stripped)
        item = re.match(r"([-*]|\d+\.) (.*)", stripped)
        if not stripped:
            flush()
            if out and out[-1]:
                out.append("")
        elif heading:
            flush()
            title = inline_text(heading.group(2))
            out.append(title)
            if len(heading.group(1)) < 3:
                out.append(("=" if len(heading.group(1)) == 1 else "-") * len(title))
        elif item and not line.startswith("  "):
            flush()
            indent = ("- " if item.group(1) in "-*" else item.group(1) + " ")
            paragraph = [item.group(2)]
        elif item:
            flush()
            indent = "  " + ("- " if item.group(1) in "-*" else item.group(1) + " ")
            paragraph = [item.group(2)]
        else:
            paragraph.append(stripped)
    flush()
    return "\n".join(out).strip() + "\n"


def write_help(path):
    text = markdown_to_text(HELP_SOURCE.read_text().replace("@VERSION@", launcher_version()))
    path.write_bytes(text.replace("\n", "\r\n").encode("ascii"))


def launcher_version():
    return re.search(r'BRRFIX_VERSION "([^"]+)"', (SOURCE / "version.h").read_text()).group(1)


def build_launcher(manifest):
    import brrpatch_build  # it imports this module
    env = watcom_env()
    objects = OUT / "obj" / "launcher"
    objects.mkdir(parents=True, exist_ok=True)
    dos = brrpatch_build.write_data()
    print(f"  the DOS patch for the launcher's button: {dos['ops']} steps, {dos['literals']:,} bytes of ours")
    data = (GAME / "WINRACE.EXE").read_bytes()
    (OUT / "game.h").write_text(
        "/* Generated by dropin-build from data/game-manifest.json: the v2.25 WINRACE.EXE as\n"
        " * installed, with its section headers corrected in place (launchers before 2026-11-15),\n"
        " * and as WINRACEP.EXE (headers corrected, timer.dll imported as PATCH.DLL, and with\n"
        " * cnc-ddraw DDRAW.dll as CNCDD.DLL; WINRACEP_PLAIN without) */\n"
        f'#define GAME_EXE_SHA256 "{manifest["sha256"]}"\n#define GAME_EXE_SIZE {manifest["size"]}u\n'
        f'#define GAME_EXE_CORRECTED_SHA256 "{corrected_sha256()}"\n'
        f'#define WINRACEP_SHA256 "{winracep_sha256()}"\n'
        f'#define WINRACEP_PLAIN_SHA256 "{winracep_sha256(False)}"\n'
        f'#define CNC_DDRAW_DLL "{CNC_DDRAW_DLL}"\n'
        f"#define WINRACEP_TIMER_OFFSET 0x{import_offset(data, TIMER_IMPORT[0]):X}u  /* \"timer.dll\" */\n"
        f"#define WINRACEP_DDRAW_OFFSET 0x{import_offset(data, DDRAW_IMPORT[0]):X}u  /* \"DDRAW.dll\" */\n")
    extract_icon((GAME / "WINRACE.EXE").read_bytes(), OUT / "launcher.ico")
    write_help(OUT / HELP_FILE)
    files = []
    for source in LAUNCHER_FILES:
        obj = objects / (source.stem + ".obj")
        forced = [f"-fi={LAUNCHER_FORCED[source.name]}"] if source.name in LAUNCHER_FORCED else []
        run_tool([str(watcom_bin() / "wcc386"), *[f for f in CFLAGS if f != "-bd"], *forced,
                  f"-i={OUT}", f"-i={SOURCE}", f"-i={LAUNCHER_SOURCE}", f"-i={BRRPATCH_SOURCE}",
                  f"-i={brrpatch_build.OUT}", f"-fo={obj}", str(source)],
                 env, f"compiling {source.name}")
        files.append(obj)
    res = objects / "launcher.res"
    run_tool([str(watcom_bin() / "wrc"), "-q", "-r", "-bt=nt", f"-i={OUT}", f"-i={SOURCE}",
              f"-i={LAUNCHER_SOURCE}", f"-i={WATCOM / 'h' / 'nt'}", f"-i={WATCOM / 'h'}",
              str(LAUNCHER_SOURCE / "launcher.rc"), f"-fo={res}"], dict(env, INCLUDE=f"{OUT}:{env['INCLUDE']}"),
             "compiling launcher.rc")
    exe = OUT / LAUNCHER
    exe.unlink(missing_ok=True)
    script = OUT / "launcher.lnk"
    script.write_text("\n".join([
        "system nt_win",
        f"name '{exe}'",
        f"option quiet, caseexact, map='{objects / 'launcher.map'}'",
        "option osversion=4.0",
        "runtime windows=4.0",
        f"option resource='{res}'",
        "library shell32.lib",
        "library comctl32.lib",
        *[f"file '{f}'" for f in files],
    ]) + "\n")
    run_tool([str(watcom_bin() / "wlink"), f"@{script}"], env, "linking the launcher")
    pe = PE(exe.read_bytes())
    extra = [name for name in pe.imports() if name.lower() not in LAUNCHER_DLLS]
    if extra:
        raise BuildError(f"the launcher imports non-system DLLs: {', '.join(extra)}")
    machine, os_version, subsystem, sub_version = pe.header()
    if machine != 0x14C or os_version != (4, 0) or sub_version != (4, 0) or subsystem != 2:
        raise BuildError(f"the launcher's PE header isn't marked for Windows 4.0: {os_version} {sub_version}")
    count = check_windows95(LAUNCHER, pe)
    print(f"  built {rel(exe)} ({exe.stat().st_size:,} bytes): imports {', '.join(pe.imports())} "
          f"({count} functions, all in Windows 95); PE headers 4.0")


def check_windows95(name, pe):
    """Every function a program imports statically must be in Windows 95: data/win95-api.json,
    the list dropin-compat checks against. Returns how many there are."""
    reference = {dll: set(names) for dll, names in json.loads(WIN95_API.read_text())["windows95"].items()}
    imported = pe.imported_functions()
    missing = [f"{dll}: {function}" for dll, functions in imported.items() for function in functions
               if function not in reference.get(dll.upper(), set())]
    if missing:
        raise BuildError(f"{name} imports functions Windows 95 doesn't have ({', '.join(missing)}). Look "
                         f"newer ones up at run time with GetProcAddress; if it's a Windows 95 function, add "
                         f"it to {rel(WIN95_API)}")
    return sum(len(functions) for functions in imported.values())


def build():
    manifest = json.loads(MANIFEST.read_text())["game"]["WINRACE.EXE"]
    exe_path = GAME / "WINRACE.EXE"
    if not exe_path.exists():
        raise BuildError("work/game/WINRACE.EXE is missing: run python3 brr.py prepare")
    data = exe_path.read_bytes()
    if sha256(data) != manifest["sha256"]:
        raise BuildError("work/game/WINRACE.EXE isn't the v2.25 file in data/game-manifest.json")
    OUT.mkdir(parents=True, exist_ok=True)
    doc = check_sites(PE(data))
    print(f"  WINRACE.EXE v2.25 checked; {len(doc['sites'])} patch sites and "
          f"{len(doc['protected'])} protected functions match")
    write_header(doc, manifest["sha256"], manifest["size"])
    write_splash()
    dll = OUT / "PATCH.DLL"
    dll.unlink(missing_ok=True)
    (OUT / "TIMER.DLL").unlink(missing_ok=True)  # the DLL's name before 2026-11-15
    compile_dll(dll)
    # Windows line endings, so Notepad on Windows 95/98 shows it properly
    text = (SOURCE / "BRRFIX.INI").read_text().replace("\r\n", "\n")
    (OUT / "BRRFIX.INI").write_bytes(text.replace("\n", "\r\n").encode("ascii"))
    text = (SOURCE / "ddraw.ini").read_text().replace("\r\n", "\n")
    (OUT / "ddraw.ini").write_bytes(text.replace("\n", "\r\n").encode("ascii"))
    check_dll(dll)
    build_dplay()
    build_launcher(manifest)
    print(f"  built {rel(dll)} ({dll.stat().st_size:,} bytes) with Open Watcom, "
          f"and {rel(OUT / 'BRRFIX.INI')}")


def build_dplay():
    """DPLAY.DLL: our stand-in for DirectX 3's DirectPlay (Windows 10 and 11 have none), which
    WINRACE.EXE imports by ordinal: 1 DirectPlayCreate, 2 DirectPlayEnumerate."""
    env = watcom_env()
    obj = OUT / "obj" / "dplay.obj"
    obj.parent.mkdir(exist_ok=True)
    run_tool([str(watcom_bin() / "wcc386"), *CFLAGS, f"-fo={obj}", str(SOURCE / "dplay.c")], env,
             "compiling dplay.c")
    dll = OUT / "DPLAY.DLL"
    dll.unlink(missing_ok=True)
    script = OUT / "dplay.lnk"
    script.write_text("\n".join([
        "system nt_dll initinstance terminstance",
        f"name '{dll}'",
        f"option quiet, caseexact, map='{OUT / 'DPLAY.map'}'",
        "option osversion=4.0",
        "runtime windows=4.0",
        f"file '{obj}'",
        "export 'DirectPlayCreate'.1='_DirectPlayCreate@12'",
        "export 'DirectPlayEnumerate'.2='_DirectPlayEnumerate@8'",
    ]) + "\n")
    run_tool([str(watcom_bin() / "wlink"), f"@{script}"], env, "linking DPLAY.DLL")
    pe = PE(dll.read_bytes())
    if pe.exports() != [(1, "DirectPlayCreate"), (2, "DirectPlayEnumerate")]:
        raise BuildError(f"DPLAY.DLL's exports aren't ordinals 1 and 2: {pe.exports()}")
    extra = [name for name in pe.imports() if name.lower() not in SYSTEM_DLLS]
    if extra:
        raise BuildError(f"DPLAY.DLL imports non-system DLLs: {', '.join(extra)}")
    print(f"  built {rel(dll)} ({dll.stat().st_size:,} bytes): exports 1:DirectPlayCreate, "
          f"2:DirectPlayEnumerate; imports {', '.join(pe.imports())}")


def check_dll(dll):
    ours = PE(dll.read_bytes())
    original = PE((GAME / "TIMER.DLL").read_bytes())
    if ours.exports() != original.exports():
        raise BuildError(f"exports differ from the original TIMER.DLL: {ours.exports()} vs "
                         f"{original.exports()}")
    extra = [name for name in ours.imports() if name.lower() not in SYSTEM_DLLS]
    if extra:
        raise BuildError(f"the DLL imports non-system DLLs: {', '.join(extra)}")
    count = check_windows95("PATCH.DLL", ours)
    machine, os_version, subsystem, sub_version = ours.header()
    if machine != 0x14C or os_version != (4, 0) or sub_version != (4, 0) or subsystem != 2:
        raise BuildError(f"PE header not marked for Windows 4.0: machine 0x{machine:X}, OS version "
                         f"{os_version}, subsystem {subsystem} version {sub_version}")
    print(f"  exports match the original: {', '.join(f'{o}:{n}' for o, n in ours.exports())}")
    print(f"  imports only system DLLs: {', '.join(ours.imports())} ({count} functions, all in Windows 95)")
    print("  PE headers: i386, OS version 4.0, Windows GUI subsystem 4.0")


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    try:
        build()
    except (BuildError, FileNotFoundError) as error:
        print(f"dropin-build: FAILED. {error}")
        return 1
    print("dropin-build: PASS")
    return 0
