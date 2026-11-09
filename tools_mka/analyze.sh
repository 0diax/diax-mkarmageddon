#!/bin/bash
# Re-run the analysis half (disasm with seeds, func_id, abi), then regenerate.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="$ROOT/work"; XBE="$ROOT/mka/game/default.xbe"
cd "$ROOT/xboxrecomp"
python -m tools.disasm "$XBE" --analysis-json "$W/xbe-analysis.json" -o "$W/disasm" --force --seed-functions "$W/seed_functions.json" > "$W/disasm.log" 2>&1
python -m tools.func_id "$XBE" --functions "$W/disasm/functions.json" --strings "$W/disasm/strings.json" --xrefs "$W/disasm/xrefs.json" -o "$W/func-id" > "$W/funcid.log" 2>&1
python -m tools.abi_analysis "$XBE" --disasm-dir "$W/disasm" --func-id-dir "$W/func-id" --output-dir "$W/abi" > "$W/abi.log" 2>&1
grep '"total_functions"' "$W/disasm/summary.json"
python "$ROOT/tools_mka/regen.py"
