# Packs the built DLLs into the zip files that Notepad++ Plugin Admin expects:
# ScrollMarks.dll at the root of the zip, one zip per architecture.
#
#   pwsh scripts/package.ps1                     # Release, every built platform
#   pwsh scripts/package.ps1 -Platforms x64
#
# Writes dist/ScrollMarks_v<version>_<arch>.zip and dist/SHA256SUMS.txt.

param(
    [string]$Configuration = "Release",
    [string[]]$Platforms = @("x64", "Win32", "ARM64"),
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$versionLine = Select-String -Path "src/Version.h" -Pattern 'SCROLLMARKS_VERSION_STRING "([^"]+)"'
if (-not $versionLine) { throw "Version not found in src/Version.h" }
$version = $versionLine.Matches[0].Groups[1].Value

$architecture = @{ "x64" = "x64"; "Win32" = "x86"; "ARM64" = "arm64" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$sums = @()

foreach ($platform in $Platforms) {
    $dll = "bin/$platform/$Configuration/ScrollMarks.dll"
    if (-not (Test-Path $dll)) {
        Write-Warning "Skipping $platform, $dll was not built"
        continue
    }

    $dllVersion = (Get-Item $dll).VersionInfo.FileVersion
    if ($dllVersion -ne $version) { throw "$dll has version $dllVersion, expected $version" }

    $stage = Join-Path $OutDir "stage-$platform"
    Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item $dll $stage
    Copy-Item "LICENSE" $stage

    $zip = Join-Path $OutDir ("ScrollMarks_v{0}_{1}.zip" -f $version, $architecture[$platform])
    Remove-Item -Force $zip -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
    Remove-Item -Recurse -Force $stage

    $hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
    $sums += "$hash  $(Split-Path -Leaf $zip)"
    Write-Host "$zip  sha256=$hash"
}

if ($sums.Count -eq 0) { throw "Nothing was packaged" }
$sums | Set-Content -Encoding ascii (Join-Path $OutDir "SHA256SUMS.txt")
