# diax-mkarmageddon

<p align="center"><b>English</b> · <a href="README.pt-BR.md">Português (Brasil)</a></p>

<p align="center">
  <a href="https://github.com/0diax/diax-mkarmageddon/releases/latest"><img src="https://img.shields.io/badge/Download-Latest-2563eb?style=for-the-badge&amp;logo=github&amp;logoColor=white" alt="Download the latest release"></a>
  <a href="https://github.com/0diax/diax-mkarmageddon/releases/latest"><img src="https://img.shields.io/github/v/release/0diax/diax-mkarmageddon?style=for-the-badge&amp;logo=github&amp;label=Latest%20release&amp;cacheSeconds=300" alt="Latest release"></a>
  <a href="https://github.com/0diax/diax-mkarmageddon/releases"><img src="https://img.shields.io/github/downloads/0diax/diax-mkarmageddon/total?style=for-the-badge&amp;logo=github&amp;label=Release%20downloads" alt="Total release downloads"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue?style=for-the-badge" alt="MIT license"></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Progress-82%25-22c55e?style=for-the-badge" alt="Progress: 82%">
</p>

**diax-mkarmageddon** is a native PC port of **Mortal Kombat: Armageddon**, made by statically recompiling the
original Xbox game.

> [!IMPORTANT]
> This project does not include the game. You need **your own ISO** of Mortal Kombat: Armageddon for the
> **original Xbox, NTSC-U (USA) release**. The game's code is generated and compiled on your PC from it.

## Contents

- [Progress](#progress)
- [Requirements](#requirements)
- [Install](#install)
- [Update](#update)
- [Features](#features)
- [HD textures](#hd-textures)
- [Known issues](#known-issues)
- [FAQ](#faq)
- [Building](#building)
- [Credits](#credits)

## Progress

```
█████████████████████████████████████████░░░░░░░░░  82%
```

The bar comes from an analysis of the generated code and from testing each mode in the game: of the 19 items
below, the working ones count in full and the partial ones count half ((14 + 3 × ½) / 19 = 82%).

**Translated code:** 30,157 functions of the game's executable. 10 call points are left without an identified
target (most with invalid addresses, false positives of the analysis), and none of them was reached in testing.

| Item | Status | Notes |
|---|---|---|
| Game code translation | ✅ Working | 30,157 functions; 10 call points without a target, never reached |
| Install from the ISO | ✅ Working | The launcher checks the disc, generates the code and compiles it |
| Graphics (Direct3D 11) | ✅ Working | Internal resolution from 480p to 4K, FXAA and SSAA |
| Movies (intro, cutscenes) | ✅ Working | |
| Controls | ✅ Working | Xbox, DualShock 4, DualSense, keyboard; mouse in the menus |
| Local 2 players | ✅ Working | The second controller joins by pressing Start |
| Profiles and saves | ✅ Working | Create a profile, sign in with its kode, save |
| Kombat (Arcade, Versus, Practice) | ✅ Working | Character select and fights |
| Konquest | 🟡 Partial | Intro, cutscenes and the start tested; the full story has not been played to the end |
| Motor Kombat | ✅ Working | Tracks, karts and races |
| Kreate a Fighter | ✅ Working | Fighter editor (body, clothes, styles) |
| The Krypt | ✅ Working | Item grid and purchases with koins |
| Extras | ✅ Working | |
| Options | ✅ Working | Gameplay, audio, video and controls |
| Widescreen 16:9 and 21:9 | 🟡 Partial | The camera opens up to widescreen; the 2D menus and HUD are stretched |
| HD textures | ✅ Working | The two community packs (see below) |
| Audio | 🟡 Partial | Music, voices and effects work; crackles have been reported and, rarely, the sound does not start |
| Online multiplayer | ❌ Not part of this version | |
| Xbox Live | ❌ Not supported | The original Xbox service no longer exists |

## Requirements

- Windows 10 or 11, 64-bit
- Graphics card with Direct3D 11
- About 10 GB free (the copied disc and the build)
- Internet for the first install (to download the build tools)
- Controller, or keyboard and mouse

## Install

1. Download the [latest release](https://github.com/0diax/diax-mkarmageddon/releases/latest):
   **`MKArmageddon-<version>-starter.zip`** (do not use GitHub's "Source code" downloads).
2. Extract the whole ZIP into a folder you can write to, for example `C:\Games\MK Armageddon`.
3. Open **`MK Armageddon.exe`**. **Language / Idioma** at the top switches it to Brazilian Portuguese.
4. Click **Choose ISO...** and select your ISO (or the `default.xbe` of a disc already extracted).
5. Click **Install and compile**. The first time, it installs what is missing through `winget`, asking first
   (Python 3 with `capstone`, CMake, Visual Studio Build Tools and Rust), copies the disc, generates the
   game's code and compiles it. It takes several minutes; at the end the launcher opens again.
6. Click **Play**.

## Update

Download the new version's ZIP and extract it into a new folder. To keep your progress, copy the `mka\save`
folder from the old installation into the new one, then choose your ISO and install again.

## Features

- **Launcher** in English or Brazilian Portuguese, with the ISO choice, the installation and the video, game
  and controller settings.
- **Internal resolution** from 480p (the Xbox's) to 4K, **anti-aliasing** FXAA or SSAA, and **V-Sync**.
- **Widescreen** 16:9 and 21:9, besides the original 4:3.
- **Borderless fullscreen** or window; F11 switches during the game.
- **Video settings inside the game**, in Options > Video (RB on the controller or V on the keyboard).
- **Xbox and PlayStation controllers** (DualShock 4 and DualSense), with button remapping, and keyboard.
- **Mouse in the menus**: in the main menu and the character select, the item under the pointer is selected.
- **Local 2 players**; with a single controller, the keyboard can be player 1.
- **HD textures** with the two community packs.
- **Skip the intro movies**.
- **Everything unlocked session**, on a copy of the saves: nothing of it reaches your profile.
- **Add to Steam** as a non-Steam game (close Steam first).

## HD textures

The game uses the two HD texture packs the community made for the PS2 version in PCSX2. They are not
included: download them from their authors' pages. Both together give the best result.

1. In the launcher, **Get HD pack (PS2)** opens
   [Mortal-Kombat-Armageddon-HD-Textures](https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures/releases)
   (`SLUS-21410.rar`; the newer versions are on the Google Drive linked there), and **Get HD remake** opens
   [Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus](https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus/releases)
   (`MK.Armageddon.Remake.zip`).
2. **Open folder** creates and opens `hd-textures`, beside `mka`.
3. Extract the downloads there as they came: the PCSX2 layout (`SLUS-21410\replacements\...`) and the
   remake's (`MK Armageddon Remake\SLUS-21543\...`) work as they are. The remake is used first and the PS2
   pack fills in the rest.
4. Open the launcher again and turn on **HD textures**.

The game recognises each texture by how it looks (the PS2's have other names) and keeps what it learned in an
index file in the pack's folder; the first start with a new pack takes longer. The packs' optional folders
(`Optional Replacements`, `Bonus Test`) and the ones starting with `_` are left out.

## Known issues

- **Audio:** crackles in the sound have been reported; in testing the audio output had no dropouts. Rarely
  the sound does not start; close the game and open it again.
- **Widescreen:** the camera opens up correctly, but the 2D menus and HUD are stretched.
- **Konquest:** the start works; the full story has not been tested to the end.
- A graphics problem may still show up: [open an issue](https://github.com/0diax/diax-mkarmageddon/issues)
  with a screenshot and the `mka\stderr.txt` file.

## FAQ

### The launcher says my disc is not the supported release

Use the original **Xbox, NTSC-U (USA)** ISO. Other regions, the PS2 version and converted images are not
accepted: the installer checks the disc's `default.xbe`.

### Where are my saves, settings and logs?

| What | Where |
|---|---|
| Saves and profiles | `mka\save` |
| Video settings | `mka\video.ini` |
| Launcher settings and language | `mka\launcher.ini` (`language=en` or `language=pt-BR`) |
| Game log | `mka\stderr.txt` |
| Installation log | `install.log`, in the ZIP's folder |

### Konquest, Krypt and Kreate a Fighter ask for a profile

In the main menu, press **L / L2 (ENTER PROFILE KODE)** and enter your profile's kode, or create one in
**PROFILES**.

### What are the keyboard controls?

| Action | Key | Controller button |
|---|---|---|
| Move | W A S D or arrows | D-pad |
| Punch | J | X / Square |
| Uppercut | U | Y / Triangle |
| Kick | K | A / Cross |
| Throw | I | B / Circle |
| Block | L or Space | RT / R2 |
| Fighting style | O | LT / L2 |
| Weapon | N | RB / R1 |
| Grab | H | LB / L1 |
| Pause | Esc | Start / Options |
| Back | Tab | Back / Share |
| Fullscreen | F11 | |
| Accelerate (Motor Kombat) | Space | |

The controller buttons can be remapped in the launcher, in **Controller buttons**.

### How do two people play?

Connect two controllers: the second player joins by pressing Start, as on the console. With a single
controller, choose **Player 1 uses: Keyboard only** in the launcher so the keyboard is player 1 and the
controller player 2.

### How can I improve performance?

Lower the **Internal resolution** and use **FXAA** instead of **SSAA** in the launcher.

## Building

You can also clone the repository and run **`Install MK Armageddon.cmd`**, which asks for the ISO and does
the same as the launcher. `scripts\build-starter.ps1` makes the release ZIP.

| Folder | Contents |
|---|---|
| `mka/src` | hand-written code: `main.c`, replacements of game functions (`recomp_manual.c`), coroutines (`mka_process.c`) |
| `mka/launcher` | the launcher (C, Win32) |
| `tools_mka` | `disc.py` (reads the ISO), `generate.py` (generates the code), function seeds and analysis tools |
| `scripts` | `install.ps1` (installation and build), `build-starter.ps1` (the release ZIP) |
| `xboxrecomp` | the Xbox static recompiler and runtime, with this project's changes (MIT license) |
| `third_party/dsp56300` | the Xbox audio DSP emulator (MIT license) |

Generated on your PC and never versioned: `mka/game` (the disc), `mka/src/recomp/gen` (the game's code),
`mka/build`, `hd-textures` (downloaded from their authors' pages) and the saves (`mka/save*`).

## Credits

This project exists thanks to the work of these people and projects:

- **[xboxrecomp](https://github.com/portingpete/xboxrecomp)**, by **sp00nz** and **portingpete**: the Xbox
  static recompiler and runtime everything here runs on (MIT license).
- **[Black 2006 Recomp](https://github.com/portingpete/Black-2006-Recomp)**, by **portingpete**: the install
  model (the player brings their own disc and the game is generated on their PC) and the runtime this project
  started from.
- **[coop-anything](https://github.com/cranebeaker/coop-anything)**, by **cranebeaker**: the reference studied
  for an online mode, which is not part of this version.
- **[OpenShao](https://github.com/OAleex/OpenShao)**, by **OAleex**: the inspiration for this page.
- **[dsp56300](https://github.com/mborgerson/dsp56300)**, by **Matt Borgerson**: the Xbox audio DSP emulation.
- **[Capstone](https://www.capstone-engine.org/)**: the disassembler the recompilation uses.
- **HD textures**:
  - **[Mortal Kombat Armageddon PCSX2 Remaster/Remake: Arts and Menus](https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus)**
    (arts, menus and characters), by **UstanakChaos**, with the help of **@Mikachuuu_** on many characters;
  - **[Mortal Kombat Armageddon HD Texture Pack](https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures)**
    (upscale of the PS2 version), by **ckiscxrsed**.

  All the credit for the textures goes to their authors. They do not come with this project: download them from
  their pages.

This project is **non-profit**: it is not sold, takes no donations and is not distributed for commercial
purposes. This project's own code is under the [MIT license](LICENSE).

Mortal Kombat is a registered trademark of Warner Bros. Entertainment Inc. Mortal Kombat: Armageddon was
developed by Midway. This is a fan project, not affiliated with Warner Bros. or Midway, and it contains no
content from the game. Use it only with an original copy you own.
