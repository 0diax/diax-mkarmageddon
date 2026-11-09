"""Seed functions whose addresses appear as code immediates.

`mov dword [esi+0x12F8], 0x00318260` stores a function pointer (the Sofdec
decoder's per-stream handlers); the detector only follows some immediate
forms. Every 32-bit immediate in detected code that is 16-byte aligned, lands
in an executable section, is not already a function start, and decodes
cleanly to a ret/jmp is seeded.

    python tools_mka/imm_seeds.py   (appends to work/seed_functions.json)
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where import load_sections, XBE, FUNCS, ROOT
from gap_seeds import plausible, data, secs, starts, md, inside
from capstone import CS_OP_IMM

md.detail = True
code = [(sva, sva + rsize, raw) for name, sva, vsize, raw, rsize in secs
        if name in (".text", "PSFD00", "PSFD_I", "PSFD_B", "PSFD_P", "XPP", "D3D", "DSOUND", "XGRPH", "XNET", "XONLINE")]
startset = set(starts)

def is_table_tail(o):
    """The 16 bytes before o decode to nothing sensible (MKA's Sofdec code
    follows its lookup tables directly)."""
    bad = sum(1 for i in md.disasm(data[o - 16:o], 0) if i.mnemonic in
              ("sahf", "lahf", "wait", "pushfd", "popfd", "in", "out", "insb", "outsb",
               "insd", "outsd", "aaa", "aas", "daa", "das", "salc", "hlt", "arpl"))
    return bad >= 3


def offset(va):
    for lo, hi, raw in code:
        if lo <= va < hi:
            return raw + va - lo
    return None

funcs = json.load(open(FUNCS))
funcs = funcs if isinstance(funcs, list) else funcs["functions"]
found = set()
for f in funcs:
    s, e = int(f["start"], 16), int(f["end"], 16)
    off = offset(s)
    if off is None:
        continue
    for insn in md.disasm(data[off:off + (e - s)], s):
        if insn.mnemonic not in ("mov", "push"):
            continue
        for op in insn.operands:
            if op.type != CS_OP_IMM:
                continue
            v = op.imm & 0xFFFFFFFF
            if v & 15 or v in startset or v in found:
                continue
            o = offset(v)
            # Inside a detected function only when the bytes before it end
            # something: int3/nop padding, a ret, or a run of table data.
            if o is None or (inside(v) and data[o - 1] not in (0xCC, 0x90, 0xC3)
                             and not is_table_tail(o)):
                continue
            if plausible(o, v):
                found.add(v)

path = os.path.join(ROOT, "work", "seed_functions.json")
seeds = json.load(open(path))
have = {s["start"].upper() for s in seeds}
new = sorted(v for v in found if ("0x%08X" % v).upper() not in have)
seeds += [{"start": "0x%08X" % v} for v in new]
json.dump(seeds, open(path, "w"), indent=0)
print("%d immediate starts, %d new; e.g. %s" % (len(found), len(new), [hex(v) for v in new[:15]]))
