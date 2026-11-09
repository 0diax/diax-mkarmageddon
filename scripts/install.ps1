[CmdletBinding()]
param([string]$Disc)

# Installs Mortal Kombat: Armageddon from the player's own disc and builds it on this PC:
#   1. the build tools, where missing (Python + capstone, CMake, Visual Studio Build Tools, Rust), through winget;
#   2. the disc (an ISO, or a folder / default.xbe of one already extracted) copied into mka\game (tools_mka\disc.py);
#   3. the game's code translated from its default.xbe (tools_mka\generate.py);
#   4. the game and the launcher compiled (CMake, Release); then the launcher opens.
# Run by "Instalar MK Armageddon.cmd", or by the launcher's "Instalar e compilar" with -Disc.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$mka = Join-Path $root 'mka'
$log = Join-Path $root 'install.log'
Start-Transcript -Path $log -Force | Out-Null

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
    $answer = Read-Host "$question [S/n]"
    return ($answer -eq '' -or $answer -match '^[sSyY]')
}
function Winget-Install($id, $what, $extra) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "Falta $what, e o winget (Instalador de Aplicativos da Microsoft Store) não está disponível para instalá-lo. Instale $what manualmente e rode de novo."
    }
    if (-not (Ask "Falta $what. Instalar agora pelo winget?")) { throw "Sem $what não é possível compilar o jogo." }
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

try {
    Refresh-Path

    Step 'Ferramentas de compilação'
    $python = Find-Python
    if (-not $python) { Winget-Install 'Python.Python.3.12' 'o Python 3'; $python = Find-Python }
    if (-not $python) { throw 'O Python 3 foi instalado mas ainda não aparece: feche esta janela e rode de novo.' }
    Invoke-Native { & $python -c "import capstone" } | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Instalando o capstone (desmontador usado pela recompilação)...'
        & $python -m pip install --user capstone
        if ($LASTEXITCODE -ne 0) { throw 'Não foi possível instalar o capstone (pip).' }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Winget-Install 'Kitware.CMake' 'o CMake' }
    if (-not (Find-VCTools)) {
        Winget-Install 'Microsoft.VisualStudio.2022.BuildTools' 'o Visual Studio Build Tools (compilador C/C++)' `
            @('--override', '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
        if (-not (Find-VCTools)) { throw 'O compilador C/C++ do Visual Studio não foi encontrado depois da instalação.' }
    }
    if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
        Winget-Install 'Rustlang.Rustup' 'o Rust (usado pelo emulador do DSP de áudio)'
        if (Get-Command rustup -ErrorAction SilentlyContinue) { & rustup default stable-msvc | Out-Null }
    }
    foreach ($tool in 'cmake', 'cargo') {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool foi instalado mas ainda não aparece: feche esta janela e rode de novo." }
    }
    Write-Host "Python ($python), capstone, CMake, Visual Studio Build Tools e Rust: ok"

    Step 'O seu disco'
    $xbe = Join-Path $mka 'game\default.xbe'
    if (-not $Disc -and -not (Test-Path $xbe)) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = 'A sua ISO do Mortal Kombat: Armageddon (Xbox original)'
        $dialog.Filter = 'Disco do Xbox (*.iso;*.xiso;default.xbe)|*.iso;*.xiso;default.xbe|Todos os arquivos|*.*'
        if ($dialog.ShowDialog() -ne 'OK') { throw 'Nenhum disco escolhido.' }
        $Disc = $dialog.FileName
    }
    if ($Disc) {
        if ((Split-Path -Leaf $Disc) -ieq 'default.xbe') { $Disc = Split-Path -Parent $Disc }
        & $python (Join-Path $root 'tools_mka\disc.py') $Disc | ForEach-Object {
            if ($_ -match '^PROGRESSO (\d+)') { Write-Progress -Activity 'Copiando o disco' -PercentComplete ([int]$Matches[1]) }
            else { Write-Host $_ }
        }
        Write-Progress -Activity 'Copiando o disco' -Completed
        if ($LASTEXITCODE -ne 0) { throw 'O disco não foi copiado (veja a mensagem acima).' }
    } else {
        Write-Host "Usando o disco já copiado em $(Join-Path $mka 'game')"
    }

    Step 'Gerando o código do jogo a partir do seu default.xbe (vários minutos)'
    & $python (Join-Path $root 'tools_mka\generate.py')
    if ($LASTEXITCODE -ne 0) { throw 'A geração do código falhou.' }

    Step 'Compilando (a primeira vez demora bastante)'
    $build = Join-Path $mka 'build'
    if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
        & cmake -S $mka -B $build -A x64
        if ($LASTEXITCODE -ne 0) { throw 'O CMake não conseguiu configurar o projeto.' }
    }
    & cmake --build $build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'A compilação falhou.' }

    if (-not (Test-Path (Join-Path $root 'hd-textures'))) {
        Step 'Texturas HD (opcional)'
        Write-Host 'No launcher: "Baixar pacote HD (PS2)" e "Baixar remake HD" abrem as páginas dos autores, e'
        Write-Host '"Abrir a pasta" mostra onde extrair os downloads.'
    }

    Step 'Pronto'
    $launcher = Join-Path $root 'MK Armageddon.exe'
    if (-not (Test-Path $launcher)) { $launcher = Join-Path $mka 'MK Armageddon.exe' }
    Write-Host "O jogo foi instalado. Abra $launcher para jogar."
    Stop-Transcript | Out-Null
    Start-Process -FilePath $launcher -WorkingDirectory $mka
    exit 0
}
catch {
    Write-Host ''
    Write-Host "ERRO: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "O registro completo está em $log"
    Stop-Transcript | Out-Null
    Read-Host 'Aperte Enter para fechar'
    exit 1
}
