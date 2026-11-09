"""Locate a guest VA: its containing detected function, and disassembly around it.

    python tools_mka/where.py 0x001C4FEE [--before 0x40] [--after 0x40]
"""
import argparse
import bisect
import json
import os
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
XBE = os.path.join(ROOT, "mka", "game", "default.xbe")
FUNCS = os.path.join(ROOT, "work", "disasm", "functions.json")


def load_sections(data):
    base = struct.unpack_from("<I", data, 0x104)[0]
    count = struct.unpack_from("<I", data, 0x11C)[0]
    hdr = struct.unpack_from("<I", data, 0x120)[0] - base
    secs = []
    for i in range(count):
        flags, va, vsize, raw, rsize, name_addr = struct.unpack_from("<6I", data, hdr + i * 56)
        n = name_addr - base
        name = data[n:data.index(b"\0", n)].decode()
        secs.append((name, va, vsize, raw, rsize))
    return secs


def to_off(secs, va):
    for name, sva, vsize, raw, rsize in secs:
        if sva <= va < sva + rsize:
            return raw + va - sva, name
    return None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("va", type=lambda s: int(s, 16))
    ap.add_argument("--before", type=lambda s: int(s, 0), default=0x30)
    ap.add_argument("--after", type=lambda s: int(s, 0), default=0x40)
    args = ap.parse_args()

    data = open(XBE, "rb").read()
    secs = load_sections(data)
    funcs = json.load(open(FUNCS))
    funcs = funcs if isinstance(funcs, list) else funcs["functions"]
    rows = sorted((int(f["start"], 16), int(f["end"], 16), f.get("name")) for f in funcs)
    starts = [r[0] for r in rows]
    i = bisect.bisect_right(starts, args.va) - 1
    for j in range(max(0, i - 1), min(len(rows), i + 2)):
        s, e, n = rows[j]
        mark = "<-- contains" if s <= args.va < e else ""
        print("%s 0x%08X..0x%08X %s" % (n, s, e, mark))

    off, sec = to_off(secs, args.va)
    print("section:", sec)
    if off is None:
        return
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    start = args.va - args.before
    o, _ = to_off(secs, start)
    for insn in md.disasm(data[o:o + args.before + args.after], start):
        mark = "  <==" if insn.address == args.va else ""
        print("0x%08X  %-20s %s %s%s" % (insn.address, insn.bytes.hex(), insn.mnemonic, insn.op_str, mark))


if __name__ == "__main__":
    main()
