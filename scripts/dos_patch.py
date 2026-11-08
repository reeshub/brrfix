#!/usr/bin/env python3
"""dos-patch: write a fixed copy of the player's own Big Red Racing v2.24 RACING.EXE.

Reads RACING.EXE (DOS, v2.24, from the official v2.25 patch), checks it is exactly that
file, and writes a new program, BRRFIX.EXE by default, beside it. The original is never
changed. The fixes (Phase 3b of the Big Red Racing port; docs/reports/phase-3b-dos-patch.md):
  - camera: the camera's per-tick steps and the follow camera's roll work per tick, not
    per frame;
  - cap: on a fast machine, at most 8 ticks of catch-up per frame (the original allows
    35, which slow machines still get);
  - interpolation: on fast machines, trucks and the follow camera are drawn between
    their last two physics positions (the physics itself is untouched);
  - fine angles: the picture turns in steps finer than 0.35 degrees (the camera stutter);
  - roll fix: the follow camera's lean doesn't flick with each step;
  - retrace: on a fast machine, each frame is shown at the start of the screen's
    vertical retrace (no tearing; at most one frame per screen refresh).
A machine is "fast" while it draws a frame in 3 ticks of 1/70 s or fewer, on average;
a 486 runs as the original.
All on by default; the options turn each off. The chosen fixes are recorded in the
version text on the main menu: "V2.24R F0.1 CIARV" means version 0.1 with the Camera,
Interpolation, fine Angles, Roll and retrace (V) fixes on.

This file runs on its own (Python 3.8 or newer, standard library only) with
dos-patches.json beside it, or as "python3 brr.py dos-patch" in the project.

Usage:
  python3 dos_patch.py [RACING.EXE] [-o BRRFIX.EXE] [options]
  python3 dos_patch.py --verify BRRFIX.EXE [--original RACING.EXE]
Options:
  --no-camera --no-interpolation --no-fine-angles --no-roll-fix --no-retrace
  --cap N            catch-up cap in ticks, 1-35 (default 8; 35 is the original)
  --force            overwrite an existing output file
Testing only (the "logging build"; never for players):
  --log              write per-tick vehicle state (BRRSTATE.CSV) and per-frame camera
                     (BRRCAM.CSV) during races, and check the draw leaves the trucks as
                     the physics left them (BRRLOG.TXT)
  --input SPEC       hold keys for race ticks: NAME:FIRST-LAST,... (NAME accel, brake,
                     left, right, or kXX for scan code XX in hex)
  --seed N           set the random seed when a race starts (repeatable runs)
  --capture N@T      keep N frames from race tick T in memory and write BRRCAP.BIN
  --prove [F-L]      at the first race frame, compare the fine-angle routine with the
                     original for every multiple of 64 (pitch steps F to L of 0-1023;
                     default all: 1024^3 combinations), in BRRLOG.TXT; the game
                     carries on afterwards
"""

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

HELP = "write a fixed copy of the player's own v2.24 DOS RACING.EXE (Phase 3b), or --verify one"

HERE = Path(__file__).resolve().parent
PAGE = 0x1000


class PatchError(Exception):
    pass


# ---------------------------------------------------------------- the LE format

class LE:
    """The parts of a DOS/4GW LE program this patcher needs: header, objects, pages and
    internal fixups. Only what RACING.EXE uses is supported (no imports, no iterated
    pages, no fixup source lists); anything else is refused."""

    def __init__(self, data):
        self.data = bytes(data)
        if self.data[:2] != b"MZ":
            raise PatchError("not a DOS program (no MZ header)")
        self.le = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[self.le:self.le + 2] != b"LE":
            raise PatchError("not a DOS/4GW program (no LE header)")
        h = self.header
        self.page_size = h(0x28)
        if self.page_size != PAGE or h(0x4C) or h(0x74) or h(0x54):
            raise PatchError("unexpected LE layout")
        self.npages = h(0x14)
        self.objects = []
        for i in range(h(0x44)):
            vsize, base, flags, first, count, _ = struct.unpack_from(
                "<6I", self.data, self.le + h(0x40) + 24 * i)
            self.objects.append({"vsize": vsize, "base": base, "flags": flags,
                                 "first": first, "count": count})
        for i in range(self.npages):
            entry = self.data[self.le + h(0x48) + 4 * i:self.le + h(0x48) + 4 * i + 4]
            if (entry[0] << 16 | entry[1] << 8 | entry[2]) != i + 1 or entry[3]:
                raise PatchError("unexpected page map")
        dp = h(0x80)
        self.pages = []
        for i in range(self.npages):
            size = self.page_size if i < self.npages - 1 else h(0x2C)
            self.pages.append(bytearray(self.data[dp + i * PAGE:dp + i * PAGE + size]))
        if dp + (self.npages - 1) * PAGE + h(0x2C) != len(self.data):
            raise PatchError("unexpected data after the last page")
        # fixups: per page, a list of records (raw bytes kept for unchanged records)
        fp, fr = self.le + h(0x68), self.le + h(0x6C)
        offsets = struct.unpack_from(f"<{self.npages + 1}I", self.data, fp)
        self.fixups = []
        for p in range(self.npages):
            records, o, end = [], fr + offsets[p], fr + offsets[p + 1]
            while o < end:
                start = o
                src, flags = self.data[o], self.data[o + 1]
                if src & 0xF0 or flags & 0xE3 or (src & 0x0F) not in (2, 5, 6, 7):
                    raise PatchError(f"unsupported fixup record in page {p + 1}")
                srcoff = struct.unpack_from("<h", self.data, o + 2)[0]
                obj = self.data[o + 4]
                o += 5
                target = 0
                if src & 0x0F != 2:
                    if flags & 0x10:
                        target = struct.unpack_from("<I", self.data, o)[0]
                        o += 4
                    else:
                        target = struct.unpack_from("<H", self.data, o)[0]
                        o += 2
                records.append({"type": src & 0x0F, "srcoff": srcoff, "obj": obj,
                                "target": target, "raw": self.data[start:o]})
            self.fixups.append(records)

    def header(self, offset):
        return struct.unpack_from("<I", self.data, self.le + offset)[0]

    # addresses here are the linker's (object base + offset), as Ghidra shows them
    def locate(self, address):
        """(object index, page index, offset in page) for a linker address in a page."""
        for n, ob in enumerate(self.objects):
            if ob["base"] <= address < ob["base"] + ob["count"] * PAGE:
                offset = address - ob["base"]
                return n, ob["first"] - 1 + offset // PAGE, offset % PAGE
        raise PatchError(f"address {address:#x} isn't in the file's pages")

    def read(self, address, length):
        out = bytearray()
        while length:
            _, page, offset = self.locate(address)
            chunk = min(length, PAGE - offset)
            piece = bytes(self.pages[page][offset:offset + chunk])
            out += piece + bytes(chunk - len(piece))
            address, length = address + chunk, length - chunk
        return bytes(out)

    def write(self, address, data):
        data = bytes(data)
        while data:
            _, page, offset = self.locate(address)
            chunk = min(len(data), PAGE - offset)
            if len(self.pages[page]) < offset + chunk:
                self.pages[page] += bytes(offset + chunk - len(self.pages[page]))
            self.pages[page][offset:offset + chunk] = data[:chunk]
            address, data = address + chunk, data[chunk:]

    def fixups_in(self, address, length):
        """Fixups whose 4 (or 2) patched bytes overlap [address, address + length)."""
        found = []
        for page, records in enumerate(self.fixups):
            page_address = self.page_address(page)
            for r in records:
                start = page_address + r["srcoff"]
                size = 2 if r["type"] == 2 else 6 if r["type"] == 6 else 4
                if start < address + length and address < start + size:
                    found.append((page, r))
        return found

    def page_address(self, page):
        for ob in self.objects:
            if ob["first"] - 1 <= page < ob["first"] - 1 + ob["count"]:
                return ob["base"] + (page - ob["first"] + 1) * PAGE
        raise PatchError(f"page {page + 1} belongs to no object")

    def object_of(self, address):
        for n, ob in enumerate(self.objects):
            if ob["base"] <= address < ob["base"] + max(ob["vsize"], ob["count"] * PAGE):
                return n
        raise PatchError(f"address {address:#x} is in no object")

    def remove_fixups(self, address, length):
        removed = 0
        for page, r in self.fixups_in(address, length):
            self.fixups[page].remove(r)
            removed += 1
        return removed

    def add_fixup(self, address, kind, target_address):
        """A 32-bit offset fixup (type 7) at a linker address, pointing at a linker
        address. Split across pages the way the Watcom linker does it."""
        assert kind == 7
        tobj = self.object_of(target_address)
        target = target_address - self.objects[tobj]["base"]
        for part in range(2):
            a = address if part == 0 else (address & ~(PAGE - 1)) + PAGE
            if part == 1 and a >= address + 4:
                break
            _, page, offset = self.locate(a)
            srcoff = offset if part == 0 else address - a
            flags = 0x10 if target > 0xFFFF else 0
            raw = struct.pack("<BBhB", 7, flags, srcoff, tobj + 1) + (
                struct.pack("<I", target) if flags else struct.pack("<H", target))
            self.fixups[page].append({"type": 7, "srcoff": srcoff, "obj": tobj + 1,
                                      "target": target, "raw": raw})

    def grow_object(self, n, new_vsize):
        """Make object n bigger. Adds zero pages to it when its pages don't cover it;
        an object's last pages may also be left out of the file (zero-filled memory)."""
        ob = self.objects[n]
        if new_vsize <= ob["vsize"]:
            return
        ob["vsize"] = new_vsize

    def add_pages(self, n, count):
        """Append count zero pages to object n (renumbering the pages after it)."""
        ob = self.objects[n]
        at = ob["first"] - 1 + ob["count"]
        if at == 0 or len(self.pages[at - 1]) < PAGE:
            self.pages[at - 1] = self.pages[at - 1].ljust(PAGE, b"\0")
        for _ in range(count):
            self.pages.insert(at, bytearray(PAGE))
            self.fixups.insert(at, [])
        ob["count"] += count
        for other in self.objects:
            if other is not ob and other["first"] - 1 >= at:
                other["first"] += count
        self.npages += count

    def build(self):
        h, d, le = self.header, self.data, self.le
        old_pagemap, old_rsrc, old_fixpage = h(0x48), h(0x50), h(0x68)
        old_fixrec, old_imp, old_dp = h(0x6C), h(0x70), h(0x80)
        objtab = bytearray()
        for ob in self.objects:
            objtab += struct.pack("<6I", ob["vsize"], ob["base"], ob["flags"], ob["first"],
                                  ob["count"], 0)
        pagemap = b"".join(bytes([0, (i + 1) >> 8 & 0xFF, (i + 1) & 0xFF, 0])
                           if i + 1 < 0x10000 else None for i in range(self.npages))
        middle = d[le + old_rsrc:le + old_fixpage]       # resources, names, entry table
        records, fixpage = bytearray(), []
        for page in self.fixups:
            fixpage.append(len(records))
            for r in page:
                records += r["raw"]
        fixpage.append(len(records))
        fixpage = struct.pack(f"<{len(fixpage)}I", *fixpage)
        tail = d[le + old_imp:old_dp]                   # import tables and padding
        tail_used = len(d[le + old_imp:le + h(0x78)]) + 1
        new_pagemap = h(0x40) + len(objtab)
        new_rsrc = new_pagemap + len(pagemap)
        delta = new_rsrc - old_rsrc
        new_fixpage = old_fixpage + delta
        new_fixrec = new_fixpage + len(fixpage)
        new_imp = new_fixrec + len(records)
        loader_end = new_imp + tail_used
        body = bytearray(d[:le + h(0x40)])
        body += objtab + pagemap + middle + fixpage + records + tail[:tail_used]
        while len(body) % 0x200:
            body.append(0)
        dp = len(body)
        for i, page in enumerate(self.pages):
            body += page if i == self.npages - 1 else bytes(page).ljust(PAGE, b"\0")
        put = lambda off, value: struct.pack_into("<I", body, le + off, value)
        put(0x14, self.npages)
        put(0x2C, len(self.pages[-1]))
        put(0x30, new_imp + tail_used - new_fixpage)          # fixup section size
        put(0x38, loader_end - h(0x40))                       # loader section size
        put(0x48, new_pagemap)
        put(0x50, new_rsrc)
        put(0x58, h(0x58) + delta)
        put(0x5C, h(0x5C) + delta)
        put(0x68, new_fixpage)
        put(0x6C, new_fixrec)
        put(0x70, new_imp)
        put(0x78, h(0x78) - old_imp + new_imp)
        put(0x80, dp)
        return bytes(body)


# ---------------------------------------------------------------- the patch

def find_patch_data(explicit=None):
    candidates = [Path(explicit)] if explicit else [
        HERE / "dos-patches.json", HERE.parent / "data" / "dos-patches.json"]
    for path in candidates:
        if path.exists():
            return json.loads(path.read_text()), path
    raise PatchError("can't find dos-patches.json (it should be beside this program)")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def parse_options(spec, args):
    """The chosen fixes, from command-line arguments, as {name: value}."""
    values = {}
    for name, about in spec["options"].items():
        values[name] = about["default"]
    if args.no_camera:
        values["camera"] = 0
    if args.no_interpolation:
        values["interpolation"] = 0
    if args.no_fine_angles:
        values["fine_angles"] = 0
    if args.no_roll_fix:
        values["roll_fix"] = 0
    if args.no_retrace:
        values["retrace"] = 0
    if args.cap is not None:
        if not 1 <= args.cap <= 35:
            raise PatchError("--cap must be between 1 and 35")
        values["cap"] = args.cap
    if args.log or args.input or args.seed is not None or args.capture or args.prove:
        values["log"] = 1
    if args.seed is not None:
        values["seed_set"], values["seed"] = 1, args.seed & 0xFFFFFFFF
    if args.capture:
        count, _, tick = args.capture.partition("@")
        values["capture_frames"], values["capture_tick"] = int(count), int(tick or 0)
    if args.prove:
        first, _, last = args.prove.partition("-")
        first, last = int(first), int(last or 1023)
        if not 0 <= first <= last <= 1023:
            raise PatchError("--prove FIRST-LAST: pitch steps 0-1023")
        values["prove"] = 1 + 2 * first + 65536 * (last + 1)
    if args.input:
        values["input"] = parse_input(spec, args.input)
    return with_hooks(values)


def with_hooks(values):
    """The race screen's hooks are needed by interpolation, fine angles, the roll fix and
    the logging build."""
    values["hooks"] = int(any(values.get(name) for name in
                              ("interpolation", "fine_angles", "roll_fix", "log")))
    values["frame_start"] = int(values.get("cap", 35) != 35 or any(
        values.get(name) for name in ("interpolation", "retrace")))
    return values


def parse_input(spec, text):
    names = spec["input_keys"]
    spans = []
    for item in text.split(","):
        name, _, ticks = item.partition(":")
        first, _, last = ticks.partition("-")
        if name.lower() in names:
            key = names[name.lower()]
        elif name[:1] in "kK" and len(name) > 1:
            key = int(name[1:], 16)
        else:
            raise PatchError(f"--input: unknown key {name!r}")
        spans.append((key, int(first), int(last or first)))
    if len(spans) > spec["input_max"]:
        raise PatchError(f"--input: at most {spec['input_max']} spans")
    return spans


def tag(spec, values):
    """The short record of the chosen fixes shown on the main menu after "V2.24R" (there's
    room for about ten characters): F and the version, one letter per fix that's on, the
    cap if it isn't the default, and L for a logging build. For example "F0.1 CIARV"."""
    letters = "".join(letter for name, letter in spec["tag_letters"].items() if values.get(name))
    cap = "" if values["cap"] == spec["options"]["cap"]["default"] else str(values["cap"])
    short = ".".join(spec["version"].split(".")[:2])
    return f"F{short} {letters}{cap}" + ("L" if values.get("log") else "")


def apply(original, spec, values):
    """The patched program, as bytes."""
    if sha256(original) != spec["input"]["sha256"]:
        raise PatchError(
            f"this isn't the v2.24 RACING.EXE from the official v2.25 patch (size "
            f"{len(original):,}, expected {spec['input']['size']:,}; SHA-256 differs). "
            "The patch only works on that exact file.")
    le = LE(original)
    code = spec["code"]
    symbols = {name: int(value, 16) for name, value in code["symbols"].items()}
    code_address, code_bytes = int(code["address"], 16), bytearray.fromhex(code["hex"])

    # our code and constant data at the end of object 1, our variables after object 3
    obj1 = le.objects[0]
    end = code_address + len(code_bytes)
    need = (end - obj1["base"] + PAGE - 1) // PAGE - obj1["count"]
    if code_address < obj1["base"] + obj1["vsize"] or end > le.objects[1]["base"]:
        raise PatchError("no room for the new code")
    le.add_pages(0, max(0, need))
    le.grow_object(0, end - obj1["base"])
    bss_address, bss_size = int(code["bss_address"], 16), code["bss_size"]
    obj3 = le.objects[2]
    if bss_address < obj3["base"] + obj3["vsize"] or bss_address + bss_size > le.objects[3]["base"]:
        raise PatchError("no room for the new variables")
    le.grow_object(2, bss_address + bss_size - obj3["base"])

    # the options, into our code's options block
    block = symbols[code["options_symbol"]]
    for name, about in spec["options"].items():
        value = values[name]
        if name in ("input", "hooks", "frame_start"):
            continue
        at = block + about["offset"] - code_address
        fmt = {1: "<B", 2: "<H", 4: "<I"}[about["size"]]
        struct.pack_into(fmt, code_bytes, at, value)
    spans = values.get("input", [])
    at = symbols[code["input_symbol"]] - code_address
    for i, (key, first, last) in enumerate(spans):
        struct.pack_into("<Iii", code_bytes, at + 12 * i, key, first, last)
    struct.pack_into("<I", code_bytes, symbols[code["input_count_symbol"]] - code_address, len(spans))
    text = tag(spec, values).encode("ascii")
    at = symbols[code["tag_symbol"]] - code_address
    if len(text) >= code["tag_size"]:
        raise PatchError("version text too long")
    code_bytes[at:at + len(text) + 1] = text + b"\0"

    # displaced instructions for trampolines, copied from the player's file
    for item in code["displaced"]:
        site = int(item["from"], 16)
        data = le.read(site, item["length"])
        if sha256(data) != item["sha256"]:
            raise PatchError(f"unexpected bytes at {site:#x}")
        at = symbols[item["into"]] - code_address
        code_bytes[at:at + item["length"]] = data
        back = site + item["length"]
        jmp_at = symbols[item["into"]] + item["length"]
        code_bytes[at + item["length"]] = 0xE9
        struct.pack_into("<i", code_bytes, at + item["length"] + 1, back - (jmp_at + 5))

    le.write(code_address, code_bytes)
    for offset, target in code["fixups"]:
        le.add_fixup(code_address + offset, 7, target)

    # the sites in the game's code
    for site in spec["sites"]:
        if not values.get(site["fix"]) and site["fix"] != "always":
            continue
        address, length = int(site["address"], 16), site["length"]
        if sha256(le.read(address, length)) != site["sha256"]:
            raise PatchError(f"unexpected bytes at site {site['name']} ({address:#x})")
        kind = site["kind"]
        if kind in ("call", "jmp"):
            target = symbols[site["target"]]
            new = bytes([0xE8 if kind == "call" else 0xE9]) + struct.pack(
                "<i", target - (address + 5))
            new += b"\x90" * (length - 5)
            le.remove_fixups(address, length)
            le.write(address, new)
        elif kind == "bytes":
            le.remove_fixups(address, length)
            le.write(address, bytes.fromhex(site["bytes"]))
        elif kind == "byte_option":
            le.write(address, bytes([values[site["option"]]]))
        elif kind == "pointer":
            # a 32-bit address in an instruction: point it (and its fixup) at our symbol
            found = le.fixups_in(address, 4)
            if len(found) != 1:
                raise PatchError(f"site {site['name']}: expected one fixup")
            le.remove_fixups(address, 4)
            le.add_fixup(address, 7, symbols[site["target"]])
        elif kind == "call_in_bytes":
            # bytes of our own, with a CALL rel32 to a symbol at a given offset
            new = bytearray.fromhex(site["bytes"])
            at = site["call_at"]
            new[at] = 0xE8
            struct.pack_into("<i", new, at + 1, symbols[site["target"]] - (address + at + 5))
            le.remove_fixups(address, length)
            le.write(address, new)
        else:
            raise PatchError(f"unknown site kind {kind}")
    return le.build()


def check_protected(original, patched, spec):
    """The code the race tick reaches must be byte-for-byte unchanged, fixups too.
    Returns a list of problems."""
    a, b = LE(original), LE(patched)
    problems = []
    for item in spec["protected"]:
        address, length = int(item["address"], 16), item["length"]
        if a.read(address, length) != b.read(address, length) or \
                sha256(b.read(address, length)) != item["sha256"]:
            problems.append(f"{item['name']} ({address:#x}) changed")
            continue
        fa = sorted((a.page_address(p) + r["srcoff"], r["type"], r["obj"], r["target"])
                    for p, r in a.fixups_in(address, length))
        fb = sorted((b.page_address(p) + r["srcoff"], r["type"], r["obj"], r["target"])
                    for p, r in b.fixups_in(address, length))
        if fa != fb:
            problems.append(f"{item['name']} ({address:#x}): its fixups changed")
    return problems


def read_tag(patched, spec):
    """The FIX... text recorded in a patched file, or None."""
    le = LE(patched)
    symbols = spec["code"]["symbols"]
    at = int(symbols[spec["code"]["tag_symbol"]], 16)
    try:
        raw = le.read(at, spec["code"]["tag_size"])
    except PatchError:
        return None
    return raw.split(b"\0")[0].decode("ascii", "replace")


def values_from_tag(spec, text, args):
    """Rebuild the options from a recorded tag (logging-build details aren't in it, so
    --verify only covers player builds exactly)."""
    words = text.split()
    short = ".".join(spec["version"].split(".")[:2])
    if not words or words[0] != f"F{short}" or len(words) > 2:
        raise PatchError(f"unexpected version text {text!r} (this patch is version {spec['version']})")
    flags = words[1] if len(words) == 2 else ""
    values = {name: about["default"] for name, about in spec["options"].items()}
    log = flags.endswith("L")
    flags = flags.rstrip("L")
    digits = "".join(ch for ch in flags if ch.isdigit())
    letters = flags[:len(flags) - len(digits)]
    for name, letter in spec["tag_letters"].items():
        values[name] = 1 if letter in letters else 0
    if digits:
        values["cap"] = int(digits)
    values["log"] = 1 if log else 0
    return with_hooks(values)


def default_paths():
    """Inside the project: work/game/RACING.EXE -> work/build/dospatch/BRRFIX.EXE."""
    root = HERE.parent
    if (root / "brr.py").exists() and (root / "work" / "game" / "RACING.EXE").exists():
        return root / "work" / "game" / "RACING.EXE", root / "work" / "build" / "dospatch" / "BRRFIX.EXE"
    return Path("RACING.EXE"), None


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="dos-patch", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     usage=argparse.SUPPRESS, add_help=True)
    parser.add_argument("input", nargs="?")
    parser.add_argument("-o", "--output")
    parser.add_argument("--data")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--verify")
    parser.add_argument("--original")
    for name in ("camera", "interpolation", "fine-angles", "roll-fix", "retrace"):
        parser.add_argument(f"--no-{name}", action="store_true")
    parser.add_argument("--cap", type=int)
    parser.add_argument("--log", action="store_true")
    parser.add_argument("--input", dest="input_spec")
    parser.add_argument("--seed", type=lambda s: int(s, 0))
    parser.add_argument("--capture")
    parser.add_argument("--prove", nargs="?", const="0-1023")
    args = parser.parse_args(argv)
    args.input, args.input_path = args.input_spec, args.input
    return args


def verify(args, spec):
    patched_path = Path(args.verify)
    original_path = Path(args.original) if args.original else patched_path.with_name("RACING.EXE")
    if not original_path.exists():
        default_in, _ = default_paths()
        original_path = default_in
    patched, original = patched_path.read_bytes(), original_path.read_bytes()
    text = read_tag(patched, spec)
    if not text:
        raise PatchError(f"{patched_path} isn't a patched file")
    values = values_from_tag(spec, text, args)
    if values.get("log"):
        raise PatchError(f"{patched_path} is a logging build ({text}): only player builds are verified")
    expected = apply(original, spec, values)
    problems = []
    if patched != expected:
        problems.append("it differs from what the patch makes from the original")
    problems += check_protected(original, patched, spec)
    LE(patched)  # parses
    if problems:
        print(f"dos-patch --verify: FAIL: {patched_path}")
        for p in problems:
            print("  " + p)
        return 1
    print(f"dos-patch --verify: PASS: {patched_path} ({text}) is exactly the patch of "
          f"{original_path} (SHA-256 {sha256(patched)[:16]}...); the {len(spec['protected'])} "
          "functions the race tick reaches are unchanged, with their fixups")
    return 0


def main(argv):
    args = parse_args(argv)
    try:
        spec, _ = find_patch_data(args.data)
        if args.verify:
            return verify(args, spec)
        default_in, default_out = default_paths()
        source = Path(args.input_path) if args.input_path else default_in
        if not source.exists():
            raise PatchError(f"can't find {source}: give the path to your RACING.EXE")
        target = Path(args.output) if args.output else (default_out or source.with_name("BRRFIX.EXE"))
        if source.resolve() == target.resolve():
            raise PatchError("the output would overwrite the original: choose another name with -o")
        if target.exists() and not args.force:
            existing = target.read_bytes()
            if existing[:2] != b"MZ" or not read_tag(existing, spec):
                raise PatchError(f"{target} exists and isn't a patched file: not overwriting it")
        values = parse_options(spec, args)
        original = source.read_bytes()
        patched = apply(original, spec, values)
        problems = check_protected(original, patched, spec)
        if problems:
            raise PatchError("internal check failed: " + "; ".join(problems))
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(patched)
        print(f"dos-patch: wrote {target} ({len(patched):,} bytes, {tag(spec, values)}) from "
              f"{source}; the original is unchanged.")
        if values.get("log"):
            print("  This is a logging build, for testing only.")
        return 0
    except PatchError as error:
        print(f"dos-patch: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
