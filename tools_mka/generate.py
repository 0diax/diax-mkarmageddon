#!/usr/bin/env python3
"""Generate Mortal Kombat: Armageddon's C code locally, from the user's own Xbox disc.

No translated game code is included in the repository. This script runs the
xboxrecomp toolkit over the user's default.xbe (taken from their own ISO by
tools_mka/disc.py) the way the verified PC build was made:

    xbe_parser -> disasm (with tools_mka/seed_functions.json) -> func_id
    -> abi_analysis -> recomp

The hand-written C (mka/src/recomp_manual.c and mka_process.c) replaces some
of the game's functions; it is passed to the
recompiler so it emits those as sub_XXXXXXXX_gen for the overrides to call.

    python tools_mka/generate.py [--xbe mka/game/default.xbe] [--output mka/src/recomp/gen]
"""
import argparse
import datetime
import filecmp
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

# The supported release: Mortal Kombat: Armageddon, original Xbox, NTSC-U.
EXPECTED_XBE = "78F99FCA447FDDC5C5116C09D27E6F1C04AF8572DACE94F559966588C6F5E393"


def command(toolkit, module, arguments, log):
    print(f"Running {module}", flush=True)
    with open(log, "a", encoding="utf-8") as out:
        out.write(f"\n== {module} {' '.join(map(str, arguments))}\n")
        out.flush()
        subprocess.run([sys.executable, "-m", module, *map(str, arguments)], cwd=toolkit, check=True,
                       stdout=out, stderr=subprocess.STDOUT)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolkit", type=Path, default=root / "xboxrecomp")
    parser.add_argument("--xbe", type=Path, default=root / "mka" / "game" / "default.xbe")
    parser.add_argument("--output", type=Path, default=root / "mka" / "src" / "recomp" / "gen")
    parser.add_argument("--keep-work", action="store_true", help="keep the intermediate analysis folder")
    args = parser.parse_args()
    toolkit, xbe, output = args.toolkit.resolve(), args.xbe.resolve(), args.output.resolve()
    if not xbe.is_file():
        parser.error("Choose your Mortal Kombat: Armageddon ISO in the launcher first (mka/game/default.xbe is missing)")
    if sha256(xbe) != EXPECTED_XBE:
        parser.error("default.xbe does not match the supported Mortal Kombat: Armageddon release (original Xbox, NTSC-U)")

    work = root / ".work" / ("generate-" + datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S"))
    work.mkdir(parents=True)
    log = work / "generate.log"
    analysis = work / "xbe-analysis.json"
    disasm, identified, abi, generated = work / "disasm", work / "func-id", work / "abi", work / "gen"

    manual = work / "manual-scan.c"
    sources = [root / "mka" / "src" / name for name in ("recomp_manual.c", "mka_process.c")]
    manual.write_text("\n".join(p.read_text(encoding="utf-8") for p in sources if p.is_file()), encoding="utf-8")

    command(toolkit, "tools.xbe_parser", [xbe, "--json", analysis], log)
    command(toolkit, "tools.disasm", [xbe, "--analysis-json", analysis, "-o", disasm, "--force",
                                      "--seed-functions", root / "tools_mka" / "seed_functions.json"], log)
    command(toolkit, "tools.func_id", [xbe, "--functions", disasm / "functions.json", "--strings",
                                       disasm / "strings.json", "--xrefs", disasm / "xrefs.json", "-o", identified], log)
    command(toolkit, "tools.abi_analysis", [xbe, "--disasm-dir", disasm, "--func-id-dir", identified,
                                            "--output-dir", abi], log)
    command(toolkit, "tools.recomp", [xbe, "--all", "--split", "1000", "--game-name", "MKA",
                                      "--disasm-dir", disasm, "--func-id-dir", identified, "--abi-dir", abi,
                                      "--gen-dir", generated, "--exclude-manual", manual,
                                      "--output-dir", work / "summary"], log)

    # Only files whose contents changed are replaced, so a rebuild stays incremental.
    output.mkdir(parents=True, exist_ok=True)
    changed = 0
    for path in sorted(generated.iterdir()):
        target = output / path.name
        if target.exists() and filecmp.cmp(path, target, shallow=False):
            continue
        shutil.copy2(path, target)
        changed += 1
    for path in output.glob("*.c"):
        if not (generated / path.name).exists():
            path.unlink()
            changed += 1
    print(f"Generated {len(list(generated.iterdir()))} files into {output} ({changed} changed)", flush=True)
    if not args.keep_work:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
