$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$compilerRoot = Join-Path $root 'compiler'
$name = 'zig-windows-x86_64-0.13.0'
$compiler = Join-Path $compilerRoot "$name\zig.exe"
if (Test-Path $compiler) {
    $version = & $compiler version
    if ($LASTEXITCODE -ne 0 -or $version.Trim() -ne '0.13.0') {
        throw 'The cached compiler is not Zig 0.13.0.'
    }
    Write-Output 'Zig 0.13.0 is ready.'
    exit 0
}
[void](New-Item -ItemType Directory -Force $compilerRoot)
$archive = Join-Path $compilerRoot "$name.zip"
$expected = 'd859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Write-Output 'Downloading portable Zig 0.13.0 (about 75 MB)...'
Invoke-WebRequest -UseBasicParsing "https://ziglang.org/download/0.13.0/$name.zip" -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    Remove-Item $archive
    throw 'Compiler archive checksum mismatch.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $compilerRoot -Force
Remove-Item $archive
if (-not (Test-Path $compiler)) { throw 'Compiler extraction failed.' }
Write-Output 'Zig 0.13.0 is ready.'
