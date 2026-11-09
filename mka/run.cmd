@echo off
rem Runs the recompiled Mortal Kombat: Armageddon with the runtime settings
rem the launcher uses (NV2A pushbuffer execution, vblank, APU, window).
cd /d "%~dp0"
set RECOMP_AC97_READY=1
set RECOMP_APU_DSP_ACK=0x83218810
rem The two audio DSPs (GP and EP) on separate threads, as on the hardware:
rem fights ~35 -> ~52 fps and about half the audio dropouts.
set RECOMP_APU_DSP_THREADS=2
set RECOMP_NV2A_KELVIN=1
set RECOMP_VBLANK=1
set RECOMP_VBLANK_KEEP_GRID=1
set RECOMP_USB=1
rem Two emulated pads (local 2 players). Player 1 = keyboard + controller 0,
rem player 2 = controller 1. With only one controller, to play keyboard (P1)
rem against controller (P2), uncomment the two lines below.
set RECOMP_USB_PADS=2
rem set RECOMP_PAD1_XINPUT=3
rem set RECOMP_PAD2_XINPUT=0
rem Keyboard as a plain controller: WASD/arrows = D-pad, J K U I = X A Y B,
rem L/Space = RT (block), O = LT, Esc = Start, Enter = A, Backspace = B, Tab = Back.
set RECOMP_KEY_LAYOUT=pad
set RECOMP_FB_WINDOW=1
set RECOMP_WINDOW_TITLE=Mortal Kombat: Armageddon
rem Skip the Midway logo and opening movies (delete this line to watch them).
set RECOMP_HIDE_FILES=moviexbox\LOGON.SFD;moviexbox\OPENINGN.SFD
rem Resolution, window/fullscreen, anti-aliasing: video.ini (F11 toggles fullscreen).
set RECOMP_VIDEO_INI=%~dp0video.ini
rem Draw on the host GPU (D3D11) instead of the CPU rasterizer: ~7 fps -> 40-60.
set RECOMP_KELVIN_GPU=1
set RECOMP_KGPU_CACHE=%~dp0kgpu-cache
if not exist kgpu-cache mkdir kgpu-cache
set RECOMP_KELVIN_THREADS=12
rem Video settings inside the game's Options > VIDEO page: press RB (controller) or V (keyboard) there.
set RECOMP_NATIVE_MENUS=0
set RECOMP_LANGUAGE=pt-BR
set RECOMP_VIDEO_MENU_HIDE=dof,ao_method,ao_quality,reflections,rt,fov,blur,crosshair,hitmarkers
rem HD textures (AI upscale of the PS2 version), matched to this version's textures
rem by appearance. The first start with them indexes the pack (takes a while).
if exist "..\hd-textures\SLUS-21410" set RECOMP_TEXTURE_PACK=%~dp0..\hd-textures\SLUS-21410
rem The remake of the arts and menus (UstanakChaos) goes first; the 1.7 pack fills in the rest.
if exist "..\hd-textures\Remake-SLUS-21543" set RECOMP_TEXTURE_PACK=%~dp0..\hd-textures\Remake-SLUS-21543;%RECOMP_TEXTURE_PACK%
rem "MK Armageddon.exe" is the same launcher with a settings window.
if not exist "build\Release\mka_recomp.exe" (
    echo O jogo ainda nao foi compilado: build\Release\mka_recomp.exe nao existe.
    pause
    exit /b 1
)
if not exist save mkdir save
"build\Release\mka_recomp.exe" %* > stdout.txt 2> stderr.txt
