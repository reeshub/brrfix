"""dropin-build: build the drop-in fix, a replacement TIMER.DLL, into work/build/dropin/.

1. Checks work/game/WINRACE.EXE is the v2.25 file from the game manifest.
2. Checks every patch site and protected range in data/dropin-patches.json against it
   by SHA-256 (recording the values the first time), and writes sites.h from them.
3. Compiles src/dropin/ with the i686 MinGW-w64 GCC in the brr-dev distrobox,
   statically, so the DLL needs only Windows system DLLs.
4. Checks the result: the same exports and ordinals as the original TIMER.DLL, and no
   imports beyond Windows system DLLs. Copies the default BRRFIX.INI beside it.
5. Compiles the files written as plain, portable C with Open Watcom as well (compile
   only), so they'll move to the Windows 95 toolchain unchanged.
Install the compiler once with:  distrobox enter brr-dev -- sudo dnf install -y mingw32-gcc
Open Watcom: the pinned snapshot in tools/open-watcom/ (see docs/KNOWLEDGE.md, Tools).
Example:
    python3 brr.py dropin-build
"""

import hashlib
import json
import os
import shutil
import struct
import subprocess

from common import BOX, DATA, ROOT, WORK, box_exists, in_box
from prepkit import rel

HELP = "build the drop-in fix (a replacement TIMER.DLL) into work/build/dropin/"

SOURCE = ROOT / "src" / "dropin"
OUT = WORK / "build" / "dropin"
PATCHES = DATA / "dropin-patches.json"
MANIFEST = DATA / "game-manifest.json"
GAME = WORK / "game"
COMPILER = "i686-w64-mingw32-gcc"
SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "winmm.dll", "msvcrt.dll",
               "advapi32.dll"}
C_FILES = ["main.c", "clock.c", "patch.c", "interp.c", "camera.c", "debug.c", "angles.c", "thunks.S",
           "timer.def"]
# Written as plain, portable C (docs/PROGRESS.md decision 2026-11-07): must compile with Open Watcom too
PORTABLE = ["angles.c"]
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


def build():
    if not box_exists():
        raise BuildError(f"the {BOX} distrobox doesn't exist; see python3 brr.py doctor")
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
    command = [COMPILER, "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-march=i686",
               "-shared", "-static-libgcc", "-static", "-s", "-I", str(OUT),
               "-o", str(dll), *[str(SOURCE / f) for f in C_FILES],
               "-Wl,--enable-stdcall-fixup", "-Wl,--dynamicbase", "-lwinmm", "-lgdi32"]
    result = subprocess.run(in_box(command), capture_output=True, text=True)
    if result.returncode != 0:
        raise BuildError("compile failed:\n" + (result.stdout + result.stderr).strip()[-4000:])
    shutil.copy2(SOURCE / "BRRFIX.INI", OUT / "BRRFIX.INI")
    check_dll(dll)
    print(f"  built {rel(dll)} ({dll.stat().st_size:,} bytes) and {rel(OUT / 'BRRFIX.INI')}")
    check_portable()


def check_portable():
    compiler = WATCOM / "binl64" / "wcc386"
    if not compiler.exists():
        raise BuildError(f"Open Watcom isn't in {rel(WATCOM)}: unpack ow-snapshot.tar.xz from "
                         f"{WATCOM_RELEASE} there (SHA-256 in docs/KNOWLEDGE.md, Tools)")
    objects = OUT / "watcom"
    objects.mkdir(exist_ok=True)
    env = dict(os.environ, WATCOM=str(WATCOM), INCLUDE=f"{WATCOM / 'h'}:{WATCOM / 'h' / 'nt'}",
               PATH=f"{WATCOM / 'binl64'}:{os.environ.get('PATH', '')}")
    for name in PORTABLE:
        obj = objects / (name.rsplit(".", 1)[0] + ".obj")
        command = [str(compiler), "-q", "-bt=nt", "-bd", "-za99", "-wx", "-we", f"-i={SOURCE}",
                   f"-fo={obj}", str(SOURCE / name)]
        result = subprocess.run(command, capture_output=True, text=True, env=env)
        if result.returncode != 0:
            raise BuildError(f"{name} doesn't compile with Open Watcom (it must stay portable C):\n"
                             + (result.stdout + result.stderr).strip()[-3000:])
    print(f"  portable C checked with Open Watcom: {', '.join(PORTABLE)}")


def check_dll(dll):
    ours = PE(dll.read_bytes())
    original = PE((GAME / "TIMER.DLL").read_bytes())
    if ours.exports() != original.exports():
        raise BuildError(f"exports differ from the original TIMER.DLL: {ours.exports()} vs "
                         f"{original.exports()}")
    extra = [name for name in ours.imports() if name.lower() not in SYSTEM_DLLS]
    if extra:
        raise BuildError(f"the DLL imports non-system DLLs: {', '.join(extra)}")
    print(f"  exports match the original: {', '.join(f'{o}:{n}' for o, n in ours.exports())}")
    print(f"  imports only system DLLs: {', '.join(ours.imports())}")


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
