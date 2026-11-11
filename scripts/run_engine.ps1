[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ($Build) {
    & (Join-Path $PSScriptRoot 'verify_engine.ps1') `
        -Configuration $Configuration `
        -SkipTests
}

$outputDirectory = Join-Path $projectRoot "x64\$Configuration"
$executable = Join-Path $outputDirectory 'DX_FrameWork.exe'
$assimpRuntime = Join-Path $outputDirectory 'assimp-vc143-mt.dll'
$assets = Join-Path $projectRoot 'assets'

foreach ($requiredPath in @($executable, $assimpRuntime, $assets)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required runtime path is missing: $requiredPath"
    }
}

Push-Location $projectRoot
try {
    & $executable
    if ($LASTEXITCODE -ne 0) {
        throw "Engine exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
