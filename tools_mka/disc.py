#!/usr/bin/env python3
"""Copy the user's own Mortal Kombat: Armageddon disc into mka/game.

    python tools_mka/disc.py [--lang en|pt-BR] "C:\\path\\Mortal Kombat Armageddon.iso"
    python tools_mka/disc.py [--lang en|pt-BR] "C:\\path\\extracted disc folder"

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
PT = False          # messages in Brazilian Portuguese (--lang pt-BR)
WRONG = ("this is not the supported release (Mortal Kombat: Armageddon, original Xbox, NTSC-U).",
         "esta não é a versão suportada (Mortal Kombat: Armageddon, Xbox original, NTSC-U).")


def t(en, pt):
    return pt if PT else en


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
    print(t("ERROR: ", "ERRO: ") + message, flush=True)
    sys.exit(1)


def from_iso(iso_path):
    try:
        with Xiso(str(iso_path)) as iso:
            xbe = iso.find("default.xbe", "")
            if xbe is None:
                fail(t("the image has no default.xbe: it is not an Xbox disc.", "a imagem não tem default.xbe: não é um disco de Xbox."))
            with tempfile.TemporaryDirectory() as tmp:
                probe = os.path.join(tmp, "default.xbe")
                iso.extract(xbe, probe)
                if sha256(probe) != EXPECTED_XBE:
                    fail(t(*WRONG))
            entries = [(d, e) for d, e in iso.walk() if wanted(((d + "/") if d else "") + e.name)]
            total = sum(e.size for _, e in entries)
            done = 0
            for d, e in entries:
                dest = GAME.joinpath(*(d.split("/") if d else []), e.name)
                if not (dest.is_file() and dest.stat().st_size == e.size):
                    iso.extract(e, str(dest))
                done += e.size
                print("PROGRESS %d" % (done * 100 // max(total, 1)), flush=True)
    except XisoError as err:
        fail(t("the image could not be read: %s", "não foi possível ler a imagem: %s") % err)


def from_folder(folder):
    xbe = folder / "default.xbe"
    if not xbe.is_file():
        fail(t("the folder has no default.xbe.", "a pasta não tem default.xbe."))
    if sha256(xbe) != EXPECTED_XBE:
        fail(t(*WRONG))
    files = [p for p in folder.rglob("*") if p.is_file() and wanted(str(p.relative_to(folder)))]
    total = sum(p.stat().st_size for p in files)
    done = 0
    for p in files:
        dest = GAME / p.relative_to(folder)
        if not (dest.is_file() and dest.stat().st_size == p.stat().st_size):
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dest)
        done += p.stat().st_size
        print("PROGRESS %d" % (done * 100 // max(total, 1)), flush=True)


def main():
    global PT
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "--lang":
        PT = args[1].lower() == "pt-br"
        args = args[2:]
    if len(args) != 1:
        fail(t("usage: disc.py [--lang en|pt-BR] <.iso file or disc folder>", "uso: disc.py [--lang en|pt-BR] <arquivo .iso ou pasta do disco>"))
    source = Path(args[0])
    GAME.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        from_folder(source)
    elif source.is_file():
        from_iso(source)
    else:
        fail(t("not found: %s", "não encontrado: %s") % source)
    print(t("OK: disc copied into %s", "OK: disco copiado para %s") % GAME, flush=True)


if __name__ == "__main__":
    main()
