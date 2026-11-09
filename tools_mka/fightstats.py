"""Summarize a fighttest.ps1 log: frame rate during the fight only.

    python tools_mka/fightstats.py mka/bench_baseline.txt [--from 95]

Frame lines ("[KELVIN] frame N: ... [qpc Q freq F]") come every 60 frames; the
first one marks t=0. Only windows that start --from seconds or later count,
which is past the menus the scripted START presses go through.
"""
import argparse
import re

ap = argparse.ArgumentParser()
ap.add_argument("log")
ap.add_argument("--from", dest="start", type=float, default=0.0)
a = ap.parse_args()

frames = []
acks, flips_wait, audio = [], [], None
crashes = 0
arena_at = None        # qpc of the frame line before the last fighter sound bank load (a fight starting)
for line in open(a.log, encoding="utf-8", errors="replace"):
    m = re.search(r"\[KELVIN\] frame (\d+):.*\[qpc (\d+) freq (\d+)\]", line)
    if m:
        frames.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
    if r"\fighter_" in line and "[PATH]" in line and frames:
        arena_at = frames[-1][1]
    m = re.search(r"no words: for PUT [\d.]+, flip ([\d.]+), ack ([\d.]+)", line)
    if m:
        flips_wait.append(float(m.group(1)))
        acks.append(float(m.group(2)))
    if "[AUDIO] elapsed" in line:
        audio = line
    if "[CRASH]" in line or "not in dispatch" in line:
        crashes += 1

if len(frames) < 3:
    print("too few frame lines:", len(frames))
    raise SystemExit
q0, f = frames[0][1], frames[0][2]
if arena_at is None:
    print("no fighter was loaded: the run never reached a fight")
    raise SystemExit
# The fight: from 8 s after the last fight load (its loading screen) on.
a.start = max(a.start, (arena_at - q0) / f + 8.0)
print(f"last fight loaded at {(arena_at - q0) / f:.0f} s; measuring from {a.start:.0f} s")
rates = []
for (n0, a0, _), (n1, a1, _) in zip(frames, frames[1:]):
    t = (a0 - q0) / f
    if t >= a.start and a1 > a0:
        rates.append((n1 - n0) / ((a1 - a0) / f))
if not rates:
    print("no windows after", a.start, "s; last frame at", (frames[-1][1] - q0) / f, "s")
    raise SystemExit
rates.sort()
mean = sum(rates) / len(rates)
good = sum(1 for r in rates if r >= 57.0) / len(rates)
print(f"fight windows: {len(rates)}  mean {mean:.1f} fps  median {rates[len(rates)//2]:.1f}  "
      f"worst {rates[0]:.1f}  >=57fps in {good*100:.0f}% of windows")
if acks:
    tail = acks[-6:]
    print(f"pusher waits per flip (last reports): ack {', '.join(f'{x:.1f}' for x in tail)} ms; "
          f"flip {', '.join(f'{x:.1f}' for x in flips_wait[-6:])} ms")
if audio:
    m = dict(re.findall(r"([a-z-]+)=([0-9]+)", audio))
    print(f"audio: empty-queue {m.get('empty-queue')} over {int(m.get('elapsed-us', 0))/1e6:.0f} s")
print("crash/bad-DPC lines:", crashes)
