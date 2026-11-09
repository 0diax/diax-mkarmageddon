#!/usr/bin/env python3
"""Copy the user's own Mortal Kombat: Armageddon disc into mka/game.

    python tools_mka/disc.py "C:\\path\\Mortal Kombat Armageddon.iso"
    python tools_mka/disc.py "C:\\path\\extracted disc folder"

An ISO is read with the toolkit's XISO reader (xboxrecomp/tools/xiso); a folder
is the disc already extracted. Either way its default.xbe has to be the
supported release (original Xbox, NTSC-U) before anything is copied. Files
already in mka/game with the same size are left as they are.
"""
import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "xboxrecomp"))
from tools.xiso.xdvdfs import Xiso, XisoError  # noqa: E402

EXPECTED_XBE = "78F99FCA447FDDC5C5116C09D27E6F1C04AF8572DACE94F559966588C6F5E393"
GAME = ROOT / "mka" / "game"
# what the game needs from the disc (the dashboard updates are not)
WANTED = ("default.xbe", "art/", "moviexbox/", "sndsxbox/")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def wanted(rel):
    rel = rel.replace("\\", "/").lower()
    return any(rel == w or rel.startswith(w) for w in WANTED)


def fail(message):
    print("ERRO: " + message, flush=True)
    sys.exit(1)


def from_iso(iso_path):
    try:
        with Xiso(str(iso_path)) as iso:
            xbe = iso.find("default.xbe", "")
            if xbe is None:
                fail("a imagem não tem default.xbe: não é um disco de Xbox.")
            with tempfile.TemporaryDirectory() as tmp:
                probe = os.path.join(tmp, "default.xbe")
                iso.extract(xbe, probe)
                if sha256(probe) != EXPECTED_XBE:
                    fail("esta não é a versão suportada (Mortal Kombat: Armageddon, Xbox original, NTSC-U).")
            entries = [(d, e) for d, e in iso.walk() if wanted(((d + "/") if d else "") + e.name)]
            total = sum(e.size for _, e in entries)
            done = 0
            for d, e in entries:
                dest = GAME.joinpath(*(d.split("/") if d else []), e.name)
                if not (dest.is_file() and dest.stat().st_size == e.size):
                    iso.extract(e, str(dest))
                done += e.size
                print("PROGRESSO %d" % (done * 100 // max(total, 1)), flush=True)
    except XisoError as err:
        fail("não foi possível ler a imagem: %s" % err)


def from_folder(folder):
    xbe = folder / "default.xbe"
    if not xbe.is_file():
        fail("a pasta não tem default.xbe.")
    if sha256(xbe) != EXPECTED_XBE:
        fail("esta não é a versão suportada (Mortal Kombat: Armageddon, Xbox original, NTSC-U).")
    files = [p for p in folder.rglob("*") if p.is_file() and wanted(str(p.relative_to(folder)))]
    total = sum(p.stat().st_size for p in files)
    done = 0
    for p in files:
        dest = GAME / p.relative_to(folder)
        if not (dest.is_file() and dest.stat().st_size == p.stat().st_size):
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dest)
        done += p.stat().st_size
        print("PROGRESSO %d" % (done * 100 // max(total, 1)), flush=True)


def main():
    if len(sys.argv) != 2:
        fail("uso: disc.py <arquivo .iso ou pasta do disco>")
    source = Path(sys.argv[1])
    GAME.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        from_folder(source)
    elif source.is_file():
        from_iso(source)
    else:
        fail("não encontrado: %s" % source)
    print("OK: disco copiado para %s" % GAME, flush=True)


if __name__ == "__main__":
    main()
