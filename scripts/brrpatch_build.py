"""brrpatch-build: build BRRPATCH.EXE, the DOS patcher for players, into work/build/brrpatch/.

1. Makes the patched RACING.EXE with the default fixes (the same as "brr.py dos-patch"
   with no options, from data/dos-patches.json and work/game/RACING.EXE), and checks the
   race tick's code is unchanged.
2. Describes the patched file as a list of steps: "copy N bytes from offset X of the
   player's RACING.EXE" or "write these N bytes of ours". Only our own bytes are stored:
   the build fails if any 16 bytes in a row of them also appear in RACING.EXE (apart from
   plain text of our own matching a common phrase, as in dos-package). Replays the steps
   to check they give exactly the patched file.
3. Writes them, with both files' sizes and SHA-256, as brrdata.h (also used by the
   launcher's "Patch the DOS version" button, built by dropin-build).
4. Compiles src/brrpatch/ with Open Watcom's 16-bit compiler as a real-mode DOS program
   (compact model, 386 instructions; no DOS extender needed), and checks it's an MZ
   program of a sensible size.
Example:
    python3 brr.py brrpatch-build
"""

import hashlib
import json
import struct
import sys

import dos_build
import dos_patch
from common import DATA, ROOT, WORK
from dropin_build import WATCOM, BuildError, run_tool, watcom_bin, watcom_env
from prepkit import rel

HELP = "build BRRPATCH.EXE, the DOS patcher for players (patches RACING.EXE in place, with a backup)"

SOURCE = ROOT / "src" / "brrpatch"
SHA_SOURCE = ROOT / "src" / "dropin"
OUT = WORK / "build" / "brrpatch"
HEADER = OUT / "brrdata.h"
EXE = OUT / "BRRPATCH.EXE"
SPEC = DATA / "dos-patches.json"
C_FILES = [SOURCE / "main.c", SOURCE / "brrpatch.c", SHA_SOURCE / "sha256.c"]
# 16-bit, compact model (far data: the steps and our bytes), 386 instructions (the game needs a 486)
CFLAGS = ["-q", "-bt=dos", "-mc", "-3", "-za99", "-s", "-os", "-w4", "-we", "-zq"]
LITERAL = 0xFFFFFFFF
MIN_MATCH = 8
MAX_CANDIDATES = 64


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def patched_game():
    """(original, patched, tag) with the default fixes, checked like dos-patch does."""
    spec = json.loads(SPEC.read_text())
    if not dos_build.GAME.exists():
        raise BuildError("work/game/RACING.EXE is missing: run python3 brr.py prepare")
    original = dos_build.GAME.read_bytes()
    values = dos_patch.with_hooks({name: about["default"] for name, about in spec["options"].items()})
    try:
        patched = dos_patch.apply(original, spec, values)
    except dos_patch.PatchError as error:
        raise BuildError(str(error)) from error
    problems = dos_patch.check_protected(original, patched, spec)
    if problems:
        raise BuildError("the race tick's code changed: " + "; ".join(problems))
    return original, patched, spec, dos_patch.tag(spec, values)


def extend(source, s, target, t):
    """How many bytes source[s:] and target[t:] have in common."""
    n, limit = 0, min(len(source) - s, len(target) - t)
    for step in (4096, 256, 16, 1):
        while n + step <= limit and source[s + n:s + n + step] == target[t + n:t + n + step]:
            n += step
    return n


def delta(source, target):
    """[(offset or LITERAL, length)] and our literal bytes, rebuilding target from source."""
    index = {}
    for s in range(len(source) - MIN_MATCH + 1):
        found = index.setdefault(source[s:s + MIN_MATCH], [])
        if len(found) < MAX_CANDIDATES:
            found.append(s)
    ops, literals, pending, t, follow = [], bytearray(), bytearray(), 0, None
    while t < len(target):
        best, best_at = 0, 0
        candidates = index.get(target[t:t + MIN_MATCH], ())
        if follow is not None:
            candidates = [follow, *candidates]
        for s in candidates:
            n = extend(source, s, target, t)
            if n > best:
                best, best_at = n, s
        if best >= MIN_MATCH:
            if pending:
                ops.append((LITERAL, len(pending)))
                literals += pending
                pending = bytearray()
            ops.append((best_at, best))
            t += best
            follow = best_at + best
        else:
            pending.append(target[t])
            t += 1
            if follow is not None:
                follow += 1
    if pending:
        ops.append((LITERAL, len(pending)))
        literals += pending
    return ops, bytes(literals)


def replay(source, ops, literals):
    out, at = bytearray(), 0
    for offset, length in ops:
        if offset == LITERAL:
            out += literals[at:at + length]
            at += length
        else:
            out += source[offset:offset + length]
    return bytes(out)


def file_offset(le, data, address, length):
    """Where length bytes at a linker address are in the file (within one page)."""
    _, page, offset = le.locate(address)
    if offset + length > dos_patch.PAGE:
        raise BuildError(f"{address:#x}: {length} bytes cross a page")
    at = le.header(0x80) + page * dos_patch.PAGE + offset
    if data[at:at + length] != le.read(address, length):
        raise BuildError(f"{address:#x}: not found at file offset {at:#x}")
    return at


def choice_offsets(patched, spec):
    """(the options block's file offset, its size, the tag's file offset) in a patched file."""
    code = spec["code"]
    le = dos_patch.LE(patched)
    symbols = {name: int(value, 16) for name, value in code["symbols"].items()}
    size = 4 * len(spec["options"])
    return (file_offset(le, patched, symbols[code["options_symbol"]], size), size,
            file_offset(le, patched, symbols[code["tag_symbol"]], code["tag_size"]))


def with_choices(patched, spec, changes):
    """The default patched file with some options changed ({name: value}), and its tag, as
    BRRPATCH writes the player's answers (for brrpatch-test)."""
    options_at, _, tag_at = choice_offsets(patched, spec)
    out = bytearray(patched)
    values = dos_patch.with_hooks({name: about["default"] for name, about in spec["options"].items()})
    values.update(changes)
    for name, value in changes.items():
        struct.pack_into("<I", out, options_at + spec["options"][name]["offset"], value)
    text = dos_patch.tag(spec, values).encode("ascii") + b"\0"
    out[tag_at:tag_at + len(text)] = text
    return bytes(out), dos_patch.tag(spec, values)


# The fixes BRRPATCH asks about (src/brrpatch/main.c), as the patch's options
QUESTION_OPTIONS = ["camera", "cap", "interpolation", "fine_angles", "roll_fix", "retrace", "menu_pad", "menu_anim"]


def choice_lines(patched, spec):
    """brrdata.h's part for BRRPATCH's questions: where the options block and the menu's tag
    are in the patched file, their default bytes (ours), each asked option's place in the
    block, and the tag's letters. BRRPATCH writes the answers there after rebuilding the
    default file, and ignores those bytes when it recognises a patched file."""
    code = spec["code"]
    options_at, size, tag_at = choice_offsets(patched, spec)
    lines = [f"#define BRR_OPTIONS_AT {options_at:#x}UL",
             f"#define BRR_OPTIONS_SIZE {size}",
             f"static const uint8_t BRR_OPTIONS_DEFAULT[BRR_OPTIONS_SIZE] = {{"
             + ",".join(str(b) for b in patched[options_at:options_at + size]) + "};"]
    for name in QUESTION_OPTIONS:
        about = spec["options"][name]
        if about["size"] != 4:
            raise BuildError(f"option {name} isn't a dword")
        lines.append(f"#define BRR_OPTION_{name.upper()} {about['offset']}")
    lines += [f"#define BRR_CAP_DEFAULT {spec['options']['cap']['default']}",
              f"#define BRR_TAG_AT {tag_at:#x}UL",
              f"#define BRR_TAG_SIZE {code['tag_size']}",
              f"static const uint8_t BRR_TAG_DEFAULT[BRR_TAG_SIZE] = {{"
              + ",".join(str(b) for b in patched[tag_at:tag_at + code["tag_size"]]) + "};",
              f'#define BRR_TAG_PREFIX "F{".".join(spec["version"].split(".")[:2])} "',
              f"#define BRR_TAG_LETTERS {len(spec['tag_letters'])}",
              "static const struct { unsigned char option; char letter; } BRR_TAG_LETTER[BRR_TAG_LETTERS] = {"
              + ", ".join(f"{{{spec['options'][name]['offset']}, '{letter}'}}" for name, letter in spec["tag_letters"].items())
              + "};"]
    return lines


def write_data():
    """Work out the steps and write brrdata.h; returns a summary dictionary."""
    from dos_package import shares_game_bytes
    original, patched, spec, tag = patched_game()
    ops, literals = delta(original, patched)
    if replay(original, ops, literals) != patched:
        raise BuildError("the steps don't rebuild the patched file")
    shared, text = shares_game_bytes(literals, original)
    if shared:
        raise BuildError(f"our bytes share 16 or more bytes in a row with RACING.EXE at {shared[:8]}")
    if len(literals) > 60000:
        raise BuildError(f"{len(literals):,} bytes of ours: more than one 16-bit segment holds")
    OUT.mkdir(parents=True, exist_ok=True)
    lines = ["/* Generated by python3 brr.py brrpatch-build from data/dos-patches.json and the",
             " * player's RACING.EXE layout: steps to rebuild the patched RACING.EXE, our own bytes",
             " * only (copies from the player's file are offsets and lengths). */",
             f'#define BRR_PATCH_VERSION "{spec["version"]}"',
             f'#define BRR_PATCH_TAG "{tag}"',
             f"#define BRR_ORIGINAL_SIZE {len(original)}UL",
             f'#define BRR_ORIGINAL_SHA256 "{sha256(original)}"',
             f"#define BRR_PATCHED_SIZE {len(patched)}UL",
             f'#define BRR_PATCHED_SHA256 "{sha256(patched)}"',
             "#define BRR_LITERAL 0xFFFFFFFFUL",
             f"#define BRR_OP_COUNT {len(ops)}",
             "static const uint32_t BRR_OPS[BRR_OP_COUNT][2] = {"]
    lines += [f"    {{{offset:#x}UL, {length}UL}}," for offset, length in ops]
    lines += ["};", f"static const uint8_t BRR_LITERALS[{len(literals)}] = {{"]
    for i in range(0, len(literals), 20):
        lines.append("    " + ",".join(str(b) for b in literals[i:i + 20]) + ",")
    lines.append("};")
    lines += choice_lines(patched, spec)
    HEADER.write_text("\n".join(lines) + "\n")
    return {"ops": len(ops), "literals": len(literals), "copied": len(patched) - len(literals),
            "patched": len(patched), "tag": tag, "text": sorted({literals[i:i + 16].decode() for i in text})}


def compile_dos():
    env = watcom_env()
    env["INCLUDE"] = str(WATCOM / "h")
    objects = OUT / "obj"
    objects.mkdir(parents=True, exist_ok=True)
    files = []
    for source in C_FILES:
        obj = objects / (source.stem + ".obj")
        run_tool([str(watcom_bin() / "wcc"), *CFLAGS, f"-i={OUT}", f"-i={SOURCE}", f"-i={SHA_SOURCE}",
                  f"-fo={obj}", str(source)], env, f"compiling {source.name}")
        files.append(obj)
    EXE.unlink(missing_ok=True)
    script = OUT / "brrpatch.lnk"
    script.write_text("\n".join([
        "system dos",
        f"name '{EXE}'",
        f"option quiet, caseexact, map='{objects / 'brrpatch.map'}'",
        "option stack=8k",
        *[f"file '{f}'" for f in files],
    ]) + "\n")
    run_tool([str(watcom_bin() / "wlink"), f"@{script}"], env, "linking BRRPATCH.EXE")
    data = EXE.read_bytes()
    if data[:2] != b"MZ" or len(data) > 200_000:
        raise BuildError(f"{rel(EXE)} isn't a plausible DOS program ({len(data):,} bytes)")
    return len(data)


def main(args):
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    try:
        summary = write_data()
        print(f"  {rel(HEADER)}: {summary['ops']} steps rebuild the {summary['patched']:,}-byte patched "
              f"RACING.EXE ({summary['tag']}): {summary['copied']:,} bytes copied from the player's file, "
              f"{summary['literals']:,} of ours (none shares 16 bytes in a row with RACING.EXE"
              + (f" except our own text: {', '.join(repr(p) for p in summary['text'])})" if summary["text"] else ")"))
        size = compile_dos()
        print(f"  built {rel(EXE)} ({size:,} bytes): 16-bit DOS, no extender")
    except (BuildError, FileNotFoundError) as error:
        print(f"brrpatch-build: FAILED. {error}", file=sys.stderr)
        return 1
    print("brrpatch-build: PASS")
    return 0
