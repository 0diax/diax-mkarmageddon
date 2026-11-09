# Scripted fight benchmark: boots MKA, presses START through the menus into an
# Arcade fight (the CPU fights an idle player), and logs frame timing.
#   powershell -File tools_mka\fighttest.ps1 -Label baseline [-Seconds 170]
# Writes mka\bench_<Label>.txt; summarize with tools_mka\fightstats.py.
param([string]$Label = "run", [int]$Seconds = 170, [string]$Extra = "")

Get-Process mka_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
$env:RECOMP_AC97_READY = '1'; $env:RECOMP_APU_DSP_ACK = '0x83218810'
$env:RECOMP_NV2A_KELVIN = '1'; $env:RECOMP_VBLANK = '1'; $env:RECOMP_VBLANK_KEEP_GRID = '1'
$env:RECOMP_USB = '1'; $env:RECOMP_USB_PADS = '1'; $env:RECOMP_FB_WINDOW = '1'
$env:RECOMP_KELVIN_THREADS = '12'; $env:RECOMP_KELVIN_GPU = '1'
$env:RECOMP_KGPU_CACHE = "$PSScriptRoot\..\mka\kgpu-cache"; $env:RECOMP_KELVIN_DUMP = ''
$env:RECOMP_KEY_LAYOUT = 'pad'; $env:RECOMP_KELVIN_TIMING = '1'; $env:RECOMP_APU_TRACE_OUTPUT = '1'
$env:RECOMP_PAD_SCRIPT = '40000:START,47000:START,54000:START,61000:START,68000:START,75000:START,82000:START'
foreach ($kv in ($Extra -split ';' | Where-Object { $_ })) {
    $k, $v = $kv -split '=', 2
    Set-Item -Path "Env:$k" -Value $v
}
Set-Location "$PSScriptRoot\..\mka"
$out = "bench_$Label.txt"
$p = Start-Process -FilePath ".\build\Release\mka_recomp.exe" -PassThru -NoNewWindow `
    -RedirectStandardError $out -RedirectStandardOutput "stdout.txt"
$h = $p.Handle
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); "ran ${Seconds}s" } else { "exited early: 0x{0:X8}" -f $p.ExitCode }
