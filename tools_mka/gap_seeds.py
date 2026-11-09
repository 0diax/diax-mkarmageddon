"""Seed function starts hiding in gaps between detected functions.

MSVC pads functions with int3 to 16-byte boundaries. An address that is
16-byte aligned, follows int3 padding, lies outside every detected function,
and decodes cleanly (no invalid/privileged instruction) up to a ret or jmp is
taken as a function start: code reached only through pointer tables, which
the detector's recursive descent never visits.

    python tools_mka/gap_seeds.py   (appends to work/seed_functions.json)
"""
import bisect, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where import load_sections, XBE, FUNCS, ROOT
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

BAD = {"in", "out", "insb", "insd", "outsb", "outsd", "hlt", "arpl", "bound", "les", "lds",
       "into", "iretd", "int1", "salc", "aam", "aad", "daa", "das", "aaa", "aas", "sldt",
       "str", "ljmp", "lcall", "retf", "wait", "pushfd", "popfd", "sahf", "lahf", "int"}

data = open(XBE, "rb").read()
secs = load_sections(data)
funcs = json.load(open(FUNCS))
funcs = funcs if isinstance(funcs, list) else funcs["functions"]
spans = sorted((int(f["start"], 16), int(f["end"], 16)) for f in funcs)
starts = [s for s, _ in spans]
md = Cs(CS_ARCH_X86, CS_MODE_32)

def inside(va):
    i = bisect.bisect_right(starts, va) - 1
    return i >= 0 and spans[i][0] <= va < spans[i][1]

def plausible(off, va):
    n = 0
    for insn in md.disasm(data[off:off + 0x400], va):
        if insn.mnemonic in BAD or insn.mnemonic == "int3":
            return False
        n += 1
        if insn.mnemonic in ("ret", "jmp") and n >= 3:
            return True
        if n > 300:
            return True
    return False

def main():
    found = []
    for name, sva, vsize, raw, rsize in secs:
        if name not in (".text", "PSFD00", "PSFD_I", "PSFD_B", "PSFD_P", "XPP", "D3D", "DSOUND", "XGRPH", "XNET", "XONLINE"):
            continue
        for va in range((sva + 15) & ~15, sva + rsize, 16):
            off = raw + va - sva
            if data[off - 1] != 0xCC or data[off] == 0xCC or va in starts or inside(va):
                continue
            if plausible(off, va):
                found.append(va)

    path = os.path.join(ROOT, "work", "seed_functions.json")
    seeds = json.load(open(path))
    have = {s["start"].upper() for s in seeds}
    new = [v for v in found if ("0x%08X" % v).upper() not in have]
    seeds += [{"start": "0x%08X" % v} for v in new]
    json.dump(seeds, open(path, "w"), indent=0)
    print("%d gap starts, %d new; e.g. %s" % (len(found), len(new), [hex(v) for v in new[:12]]))


if __name__ == "__main__":
    main()
