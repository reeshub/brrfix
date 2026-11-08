"""dropin-build: build the drop-in fix, a replacement TIMER.DLL, into work/build/dropin/.

1. Checks work/game/WINRACE.EXE is the v2.25 file from the game manifest.
2. Checks every patch site and protected range in data/dropin-patches.json against it
   by SHA-256 (recording the values the first time), and writes sites.h from them.
3. Compiles src/dropin/ with Open Watcom v2 (the pinned snapshot in tools/open-watcom/,
   see docs/KNOWLEDGE.md, Tools) for Windows 95 and later: 486 instructions, its own C
   runtime linked in, so the DLL needs only Windows system DLLs (not msvcrt.dll).
4. Checks the result: the same exports and ordinals as the original TIMER.DLL, imports
   from Windows system DLLs only, and PE headers marked for Windows 4.0. Copies the
   default BRRFIX.INI beside it.
Example:
    python3 brr.py dropin-build
"""

import hashlib
import json
import os
import shutil
import struct
import subprocess
import urllib.request

from common import DATA, ROOT, WORK
from prepkit import rel

HELP = "build the drop-in fix (a replacement TIMER.DLL) into work/build/dropin/"

SOURCE = ROOT / "src" / "dropin"
OUT = WORK / "build" / "dropin"
PATCHES = DATA / "dropin-patches.json"
MANIFEST = DATA / "game-manifest.json"
GAME = WORK / "game"
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "winmm.dll", "advapi32.dll"}
C_FILES = ["main.c", "clock.c", "patch.c", "interp.c", "camera.c", "debug.c", "angles.c", "display.c",
           "music.c", "decoders.c"]
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
             f"#define EXPECTED_EXE_SIZE {exe_size}u",
             "static const struct site_info SITES[] = {"]
    for s in doc["sites"]:
        lines.append(f'    {{"{s["name"]}", "{s["fix"]}", {s["address"]}u, {s["length"]}u, "{s["sha256"]}"}},')
    lines += ["};", "static const struct range_info PROTECTED[] = {"]
    for r in doc["protected"]:
        lines.append(f'    {{"{r["name"]}", {r["address"]}u, {r["size"]}u, "{r["sha256"]}"}},')
    lines.append("};")
    (OUT / "sites.h").write_text("\n".join(lines) + "\n")


def watcom_env():
    if not (WATCOM / "binl64" / "wcc386").exists():
        raise BuildError(f"Open Watcom isn't in {rel(WATCOM)}: unpack ow-snapshot.tar.xz from "
                         f"{WATCOM_RELEASE} there (SHA-256 in docs/KNOWLEDGE.md, Tools)")
    return dict(os.environ, WATCOM=str(WATCOM), INCLUDE=f"{WATCOM / 'h'}:{WATCOM / 'h' / 'nt'}",
                PATH=f"{WATCOM / 'binl64'}:{os.environ.get('PATH', '')}")


def run_tool(command, env, what):
    result = subprocess.run(command, capture_output=True, text=True, env=env)
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
    includes = [f"-i={folder}" for folder in fetch_decoders()]
    objects = OUT / "obj"
    objects.mkdir(exist_ok=True)
    files = []
    for name in C_FILES:
        obj = objects / (name.rsplit(".", 1)[0] + ".obj")
        flags = [f for f in CFLAGS if f not in ("-w4", "-we")] + ["-w0"] if name in QUIET_FILES else CFLAGS
        run_tool([str(WATCOM / "binl64" / "wcc386"), *flags, f"-i={OUT}", f"-i={SOURCE}", *includes,
                  f"-fo={obj}", str(SOURCE / name)], env, f"compiling {name}")
        files.append(obj)
    for name in ASM_FILES:
        obj = objects / (name.rsplit(".", 1)[0] + ".obj")
        run_tool([str(WATCOM / "binl64" / "wasm"), "-q", "-4", f"-fo={obj}", str(SOURCE / name)],
                 env, f"assembling {name}")
        files.append(obj)
    script = OUT / "timer.lnk"
    script.write_text("\n".join([
        "system nt_dll initinstance terminstance",
        f"name '{dll}'",
        f"option quiet, caseexact, map='{OUT / 'TIMER.map'}'",
        "option osversion=4.0",
        "runtime windows=4.0",
        "library winmm.lib",
        *[f"file '{f}'" for f in files],
        *[f"export '{name}'.{ordinal}={symbol}" for ordinal, name, symbol in EXPORTS],
    ]) + "\n")
    run_tool([str(WATCOM / "binl64" / "wlink"), f"@{script}"], env, "linking")


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
    dll = OUT / "TIMER.DLL"
    dll.unlink(missing_ok=True)
    compile_dll(dll)
    # Windows line endings, so Notepad on Windows 95/98 shows it properly
    text = (SOURCE / "BRRFIX.INI").read_text().replace("\r\n", "\n")
    (OUT / "BRRFIX.INI").write_bytes(text.replace("\n", "\r\n").encode("ascii"))
    text = (SOURCE / "ddraw.ini").read_text().replace("\r\n", "\n")
    (OUT / "ddraw.ini").write_bytes(text.replace("\n", "\r\n").encode("ascii"))
    check_dll(dll)
    print(f"  built {rel(dll)} ({dll.stat().st_size:,} bytes) with Open Watcom, "
          f"and {rel(OUT / 'BRRFIX.INI')}")


def check_dll(dll):
    ours = PE(dll.read_bytes())
    original = PE((GAME / "TIMER.DLL").read_bytes())
    if ours.exports() != original.exports():
        raise BuildError(f"exports differ from the original TIMER.DLL: {ours.exports()} vs "
                         f"{original.exports()}")
    extra = [name for name in ours.imports() if name.lower() not in SYSTEM_DLLS]
    if extra:
        raise BuildError(f"the DLL imports non-system DLLs: {', '.join(extra)}")
    machine, os_version, subsystem, sub_version = ours.header()
    if machine != 0x14C or os_version != (4, 0) or sub_version != (4, 0) or subsystem != 2:
        raise BuildError(f"PE header not marked for Windows 4.0: machine 0x{machine:X}, OS version "
                         f"{os_version}, subsystem {subsystem} version {sub_version}")
    print(f"  exports match the original: {', '.join(f'{o}:{n}' for o, n in ours.exports())}")
    print(f"  imports only system DLLs: {', '.join(ours.imports())}")
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
