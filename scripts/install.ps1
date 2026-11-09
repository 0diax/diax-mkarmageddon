[CmdletBinding()]
param([string]$Disc, [string]$Lang)

# Installs Mortal Kombat: Armageddon from the player's own disc and builds it on this PC:
#   1. the build tools, where missing (Python + capstone, CMake, Visual Studio Build Tools, Rust), through winget;
#   2. the disc (an ISO, or a folder / default.xbe of one already extracted) copied into mka\game (tools_mka\disc.py);
#   3. the game's code translated from its default.xbe (tools_mka\generate.py);
#   4. the game and the launcher compiled (CMake, Release); then the launcher opens.
# Run by "Install MK Armageddon.cmd", or by the launcher's "Install and compile" with -Disc and -Lang.
# -Lang en (default) or pt-BR; without it, the launcher's choice in mka\launcher.ini.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$mka = Join-Path $root 'mka'
$log = Join-Path $root 'install.log'
if (-not $Lang) {
    $ini = Join-Path $mka 'launcher.ini'
    $Lang = if ((Test-Path $ini) -and (Select-String -Path $ini -Pattern '^language=pt-BR' -Quiet)) { 'pt-BR' } else { 'en' }
}
$pt = $Lang -ieq 'pt-BR'
Start-Transcript -Path $log -Force | Out-Null

function T($en, $ptText) { if ($pt) { $ptText } else { $en } }
function Step($text) { Write-Host ''; Write-Host "== $text" -ForegroundColor Yellow }
# A native command's output, its stderr dropped. Windows PowerShell 5.1 turns a native command's stderr into a
# terminating error under $ErrorActionPreference = 'Stop', so probes go through here.
function Invoke-Native([scriptblock]$Command) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $out = & $Command 2>$null } finally { $ErrorActionPreference = $previous }
    return $out
}
function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    $cargo = Join-Path $env:USERPROFILE '.cargo\bin'
    if (Test-Path $cargo) { $env:Path += ";$cargo" }
}
function Ask($question) {
    $answer = Read-Host "$question $(T '[Y/n]' '[S/n]')"
    return ($answer -eq '' -or $answer -match '^[sSyY]')
}
function Winget-Install($id, $what, $extra) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw (T "$what is missing, and winget (the Microsoft Store's App Installer) is not available to install it. Install $what yourself and run this again." `
                 "Falta $what, e o winget (Instalador de Aplicativos da Microsoft Store) não está disponível para instalá-lo. Instale $what manualmente e rode de novo.")
    }
    if (-not (Ask (T "$what is missing. Install it now with winget?" "Falta $what. Instalar agora pelo winget?"))) {
        throw (T "The game cannot be compiled without $what." "Sem $what não é possível compilar o jogo.")
    }
    $arguments = @('install', '--id', $id, '-e', '--accept-package-agreements', '--accept-source-agreements')
    if ($extra) { $arguments += $extra }
    & winget @arguments
    Refresh-Path
}
function Find-Python {
    foreach ($candidate in @('py', 'python')) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if (-not $cmd -or $cmd.Source -like '*WindowsApps*') { continue }
        $version = Invoke-Native { & $candidate -c "import sys; print(sys.version_info >= (3, 10))" }
        if ($version -eq 'True') { return $candidate }
    }
    return $null
}
function Find-VCTools {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    return & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$restart = T 'was installed but does not show up yet: close this window and run it again.' 'foi instalado mas ainda não aparece: feche esta janela e rode de novo.'

try {
    Refresh-Path

    Step (T 'Build tools' 'Ferramentas de compilação')
    $python = Find-Python
    if (-not $python) { Winget-Install 'Python.Python.3.12' 'Python 3'; $python = Find-Python }
    if (-not $python) { throw "Python 3 $restart" }
    Invoke-Native { & $python -c "import capstone" } | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host (T 'Installing capstone (the disassembler the recompilation uses)...' 'Instalando o capstone (desmontador usado pela recompilação)...')
        & $python -m pip install --user capstone
        if ($LASTEXITCODE -ne 0) { throw (T 'capstone could not be installed (pip).' 'Não foi possível instalar o capstone (pip).') }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Winget-Install 'Kitware.CMake' 'CMake' }
    if (-not (Find-VCTools)) {
        Winget-Install 'Microsoft.VisualStudio.2022.BuildTools' (T 'Visual Studio Build Tools (C/C++ compiler)' 'Visual Studio Build Tools (compilador C/C++)') `
            @('--override', '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
        if (-not (Find-VCTools)) { throw (T "Visual Studio's C/C++ compiler was not found after the installation." 'O compilador C/C++ do Visual Studio não foi encontrado depois da instalação.') }
    }
    if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
        Winget-Install 'Rustlang.Rustup' (T 'Rust (used by the audio DSP emulator)' 'Rust (usado pelo emulador do DSP de áudio)')
        if (Get-Command rustup -ErrorAction SilentlyContinue) { & rustup default stable-msvc | Out-Null }
    }
    foreach ($tool in 'cmake', 'cargo') {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool $restart" }
    }
    Write-Host "Python ($python), capstone, CMake, Visual Studio Build Tools, Rust: ok"

    Step (T 'Your disc' 'O seu disco')
    $xbe = Join-Path $mka 'game\default.xbe'
    if (-not $Disc -and -not (Test-Path $xbe)) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = T 'Your Mortal Kombat: Armageddon ISO (original Xbox)' 'A sua ISO do Mortal Kombat: Armageddon (Xbox original)'
        $dialog.Filter = T 'Xbox disc (*.iso;*.xiso;default.xbe)|*.iso;*.xiso;default.xbe|All files|*.*' 'Disco do Xbox (*.iso;*.xiso;default.xbe)|*.iso;*.xiso;default.xbe|Todos os arquivos|*.*'
        if ($dialog.ShowDialog() -ne 'OK') { throw (T 'No disc chosen.' 'Nenhum disco escolhido.') }
        $Disc = $dialog.FileName
    }
    if ($Disc) {
        if ((Split-Path -Leaf $Disc) -ieq 'default.xbe') { $Disc = Split-Path -Parent $Disc }
        $copying = T 'Copying the disc' 'Copiando o disco'
        & $python (Join-Path $root 'tools_mka\disc.py') --lang $Lang $Disc | ForEach-Object {
            if ($_ -match '^PROGRESS (\d+)') { Write-Progress -Activity $copying -PercentComplete ([int]$Matches[1]) }
            else { Write-Host $_ }
        }
        Write-Progress -Activity $copying -Completed
        if ($LASTEXITCODE -ne 0) { throw (T 'The disc was not copied (see the message above).' 'O disco não foi copiado (veja a mensagem acima).') }
    } else {
        Write-Host (T "Using the disc already copied into $(Join-Path $mka 'game')" "Usando o disco já copiado em $(Join-Path $mka 'game')")
    }

    Step (T "Generating the game's code from your default.xbe (several minutes)" 'Gerando o código do jogo a partir do seu default.xbe (vários minutos)')
    & $python (Join-Path $root 'tools_mka\generate.py')
    if ($LASTEXITCODE -ne 0) { throw (T 'The code generation failed.' 'A geração do código falhou.') }

    Step (T 'Compiling (the first time takes a while)' 'Compilando (a primeira vez demora bastante)')
    $build = Join-Path $mka 'build'
    if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
        & cmake -S $mka -B $build -A x64
        if ($LASTEXITCODE -ne 0) { throw (T 'CMake could not configure the project.' 'O CMake não conseguiu configurar o projeto.') }
    }
    & cmake --build $build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw (T 'The compilation failed.' 'A compilação falhou.') }

    if (-not (Test-Path (Join-Path $root 'hd-textures'))) {
        Step (T 'HD textures (optional)' 'Texturas HD (opcional)')
        Write-Host (T 'In the launcher, "Get HD pack (PS2)" and "Get HD remake" open the authors'' pages, and "Open folder" shows where to extract the downloads.' `
                      'No launcher, "Baixar pacote HD (PS2)" e "Baixar remake HD" abrem as páginas dos autores, e "Abrir a pasta" mostra onde extrair os downloads.')
    }

    Step (T 'Done' 'Pronto')
    $launcher = Join-Path $root 'MK Armageddon.exe'
    if (-not (Test-Path $launcher)) { $launcher = Join-Path $mka 'MK Armageddon.exe' }
    Write-Host (T "The game is installed. Open $launcher to play." "O jogo foi instalado. Abra $launcher para jogar.")
    Stop-Transcript | Out-Null
    Start-Process -FilePath $launcher -WorkingDirectory $mka
    exit 0
}
catch {
    Write-Host ''
    Write-Host "$(T 'ERROR' 'ERRO'): $($_.Exception.Message)" -ForegroundColor Red
    Write-Host (T "The full log is in $log" "O registro completo está em $log")
    Stop-Transcript | Out-Null
    Read-Host (T 'Press Enter to close' 'Aperte Enter para fechar')
    exit 1
}
