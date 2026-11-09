@echo off
rem Escolha a sua ISO do Mortal Kombat: Armageddon (Xbox original): o jogo e gerado e compilado neste PC.
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install.ps1"
