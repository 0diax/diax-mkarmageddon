"""Regenerate MKA's C code and copy over only what changed.

    python tools_mka/regen.py

Every hand-written override (recomp_manual.c, mka_process.c) is passed to the
recompiler so it leaves those functions out. Regeneration rewrites every file,
which would make MSBuild recompile all ~3.9M lines; copying only files whose
contents changed keeps the rebuild incremental. recomp_funcs.h is included by
every generated file, so it is only replaced when it gained a declaration --
a declaration for a function that moved to the manual files is harmless.
"""
import filecmp
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLKIT = os.path.join(ROOT, "xboxrecomp")
WORK = os.path.join(ROOT, "work")
GEN = os.path.join(ROOT, "mka", "src", "recomp", "gen")
NEW = os.path.join(WORK, "gen_new")
MANUAL = [os.path.join(ROOT, "mka", "src", n) for n in ("recomp_manual.c", "mka_process.c")]


def main():
    flat = os.path.join(WORK, "manual-scan.c")
    with open(flat, "w", encoding="utf-8") as out:
        for path in MANUAL:
            out.write(open(path, encoding="utf-8").read() + "\n")
    if os.path.isdir(NEW):
        shutil.rmtree(NEW)
    subprocess.run([sys.executable, "-m", "tools.recomp",
                    os.path.join(ROOT, "mka", "game", "default.xbe"),
                    "--all", "--split", "1000", "--game-name", "MKA",
                    "--disasm-dir", os.path.join(WORK, "disasm"),
                    "--func-id-dir", os.path.join(WORK, "func-id"),
                    "--abi-dir", os.path.join(WORK, "abi"),
                    "--gen-dir", NEW, "--exclude-manual", flat,
                    "--output-dir", os.path.join(WORK, "summary")],
                   cwd=TOOLKIT, check=True,
                   stdout=open(os.path.join(WORK, "recomp_last.log"), "w"),
                   stderr=subprocess.STDOUT)

    changed = []
    for name in sorted(os.listdir(NEW)):
        src, dst = os.path.join(NEW, name), os.path.join(GEN, name)
        if os.path.exists(dst) and filecmp.cmp(src, dst, shallow=False):
            continue
        if name == "recomp_funcs.h" and os.path.exists(dst):
            old = set(open(dst, encoding="utf-8").read().splitlines())
            gained = [l for l in open(src, encoding="utf-8").read().splitlines()
                      if l.startswith("void ") and l not in old]
            if not gained:
                continue
        shutil.copy2(src, dst)
        changed.append(name)
    for name in sorted(os.listdir(GEN)):
        if name.endswith(".c") and not os.path.exists(os.path.join(NEW, name)):
            os.remove(os.path.join(GEN, name))
            changed.append(name + " (removed)")
    print("changed:", ", ".join(changed) if changed else "nothing")


if __name__ == "__main__":
    main()
