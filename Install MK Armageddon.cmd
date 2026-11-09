@echo off
rem Choose your Mortal Kombat: Armageddon ISO (original Xbox): the game is generated and compiled on this PC.
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install.ps1"
