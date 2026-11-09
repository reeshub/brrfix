"""win-headers: check WINRACE.EXE's section headers, and write a copy that modern Windows accepts.

The 1996 Watcom linker left every section's VirtualSize at 0, and gave the uninitialised
data section (.bss) a SizeOfRawData of about 900 KB with no data in the file. Windows 95
and Wine load that; the suspicion (likely, not yet confirmed) is that Windows NT-family
loaders refuse it ("This app can't run on your PC"). This command lists such problems
and writes work/build/winheaders/WINRACE.EXE with only the section headers corrected:
each VirtualSize set to the size Windows 95 used, and the .bss section's SizeOfRawData
set to 0. It checks that both files give the same program in memory. A test tool for
the director's Windows 10 PC: the copy is game content, so it stays in work/.
Example:
    python3 brr.py win-headers
"""

import struct

from common import WORK
from prepkit import rel

HELP = "check WINRACE.EXE's section headers and write a header-corrected test copy (Windows 10 problem)"

SOURCE = WORK / "game" / "WINRACE.EXE"
OUT = WORK / "build" / "winheaders"
UNINITIALISED = 0x80


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count, opt_size = struct.unpack_from("<H12xH", data, pe + 6)
    table = pe + 24 + opt_size
    found = []
    for i in range(count):
        at = table + 40 * i
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, at + 8)
        flags = struct.unpack_from("<I", data, at + 36)[0]
        found.append({"at": at, "name": data[at:at + 8].rstrip(b"\0").decode("latin-1"), "vsize": vsize,
                      "va": va, "rsize": rsize, "raw": raw, "flags": flags})
    size_of_image, size_of_headers = struct.unpack_from("<II", data, pe + 24 + 56)
    return found, size_of_image, size_of_headers


def memory_image(data):
    """The program as a lenient loader (Windows 95, Wine) maps it."""
    found, size_of_image, size_of_headers = sections(data)
    image = bytearray(size_of_image)
    for s in found:
        size = s["vsize"] or s["rsize"]
        if s["raw"] and not s["flags"] & UNINITIALISED:
            chunk = data[s["raw"]:s["raw"] + min(s["rsize"], size)]
            image[s["va"]:s["va"] + len(chunk)] = chunk
    return bytes(image[size_of_headers:])


def corrected(data):
    """(the header-corrected file, [(section name, problems)])."""
    fixed = bytearray(data)
    report = []
    for s in sections(data)[0]:
        problems = []
        if s["vsize"] == 0:
            problems.append("VirtualSize 0")
            struct.pack_into("<I", fixed, s["at"] + 8, s["rsize"])
        if s["rsize"] and s["raw"] + s["rsize"] > len(data):
            problems.append(f"raw data 0x{s['raw']:X}+0x{s['rsize']:X} runs past the end of the "
                            f"file (0x{len(data):X} bytes)")
        if s["flags"] & UNINITIALISED and s["rsize"]:
            struct.pack_into("<I", fixed, s["at"] + 16, 0)
            struct.pack_into("<I", fixed, s["at"] + 20, 0)
        report.append((s, problems))
    return bytes(fixed), report


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if not SOURCE.exists():
        print("win-headers: work/game/WINRACE.EXE is missing: run python3 brr.py prepare")
        return 1
    data = SOURCE.read_bytes()
    fixed, report = corrected(data)
    for s, problems in report:
        print(f"  {s['name']:9} at 0x{s['va']:06X}: " + ("; ".join(problems) if problems else "fine"))
    if memory_image(data) != memory_image(fixed):
        print("win-headers: FAILED: the corrected copy would load differently")
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "WINRACE.EXE").write_bytes(fixed)
    changed = sum(a != b for a, b in zip(data, fixed))
    print(f"  wrote {rel(OUT / 'WINRACE.EXE')}: {changed} header bytes changed, the same program in memory")
    print("win-headers: done")
    return 0
