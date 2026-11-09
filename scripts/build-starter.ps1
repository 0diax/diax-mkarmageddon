[CmdletBinding()]
param()

# Builds the starter ZIP (the release download players use): the launcher already compiled, beside every source
# it needs to build the game (this repository's, xboxrecomp's and dsp56300's, as plain folders: no git, no
# submodules), START AQUI.txt and SHA256SUMS. Never any game file: the player brings their own disc.
# Made from what is committed (git archive), with the launcher compiled from that same copy.
#   .\scripts\build-starter.ps1   ->  .work\starter\MKArmageddon-<VERSION>-starter.zip
$ErrorActionPreference = 'Stop'
$env:MSBUILDDISABLENODEREUSE = '1'      # no idle MSBuild nodes left holding the staging folder
$root = Split-Path -Parent $PSScriptRoot
$version = (Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
$name = "MKArmageddon-$version"
$out = Join-Path $root '.work\starter'
$stage = Join-Path $out $name
$buildDir = Join-Path $out 'launcher-build'
$zip = Join-Path $out "$name-starter.zip"
foreach ($path in $stage, $buildDir, $zip) { if (Test-Path $path) { Remove-Item -Recurse -Force $path } }
New-Item -ItemType Directory -Force $stage | Out-Null

function Export-Tree($repo, $into) {
    $tar = Join-Path $out 'export.tar'
    & git -C $repo archive --format=tar -o $tar HEAD
    if ($LASTEXITCODE -ne 0) { throw "git archive falhou em $repo" }
    New-Item -ItemType Directory -Force $into | Out-Null
    & tar.exe -xf $tar -C $into
    if ($LASTEXITCODE -ne 0) { throw "não foi possível extrair $tar" }
    Remove-Item $tar
}
Write-Host 'Copiando o código (o que está commitado)...'
Export-Tree $root $stage
Export-Tree (Join-Path $root 'xboxrecomp') (Join-Path $stage 'xboxrecomp')
Export-Tree (Join-Path $root 'third_party\dsp56300') (Join-Path $stage 'third_party\dsp56300')

Write-Host 'Compilando o launcher...'
& cmake -S (Join-Path $stage 'mka') -B $buildDir -A x64 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'O CMake não conseguiu configurar o projeto.' }
& cmake --build $buildDir --config Release --target mka_launcher | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'A compilação do launcher falhou.' }
Remove-Item -Force (Join-Path $stage 'mka\MK Armageddon.exe')   # the package's copy is the one at its top
if (-not (Test-Path (Join-Path $stage 'MK Armageddon.exe'))) { throw 'O launcher não foi copiado para o pacote.' }
foreach ($leftover in 'mka\src\recomp', 'hd-textures', '.gitmodules') { $p = Join-Path $stage $leftover; if (Test-Path $p) { Remove-Item -Recurse -Force $p } }
# (xboxrecomp's own tools\conformance\test.xbe is the toolkit's test program, not a game's)
$forbidden = Get-ChildItem -Recurse $stage -Include '*.iso', '*.xiso', 'default.xbe' -File
if ($forbidden) { throw "O pacote tem arquivos do jogo, que não podem ir: $($forbidden.FullName -join ', ')" }

Write-Host 'Somas SHA-256...'
$sums = Get-ChildItem -Recurse -File $stage | Sort-Object FullName | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower(), $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
}
[IO.File]::WriteAllLines((Join-Path $stage 'SHA256SUMS'), $sums)

Write-Host 'Compactando...'
Compress-Archive -Path $stage -DestinationPath $zip
Remove-Item -Recurse -Force $buildDir
Write-Host ("Pronto: {0} ({1:N1} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
