[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',
    [switch]$Rebuild,
    [switch]$SkipEngine,
    [switch]$SkipTests,
    [switch]$SkipShaders
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$engineProject = Join-Path $projectRoot 'DX_FrameWork.vcxproj'
$testProject = Join-Path $projectRoot 'tests\DXFrameworkCoreTests.vcxproj'

function Find-MSBuild {
    $knownLocations = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
    )

    foreach ($candidate in $knownLocations) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw 'MSBuild.exe was not found. Install Visual Studio 2022 with Desktop development with C++.'
}

function Invoke-Build {
    param(
        [string]$MSBuild,
        [string]$Project,
        [string]$BuildConfiguration
    )

    $target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
    Write-Host "==> $target $([IO.Path]::GetFileName($Project)) [$BuildConfiguration|x64]"
    & $MSBuild $Project "/t:$target" "/p:Configuration=$BuildConfiguration" '/p:Platform=x64' '/m:1' '/nologo' '/verbosity:minimal'
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $Project ($BuildConfiguration|x64)"
    }
}

$configurations = if ($Configuration -eq 'All') {
    @('Debug', 'Release')
} else {
    @($Configuration)
}

$msbuild = Find-MSBuild
Push-Location $projectRoot
try {
    if (-not $SkipShaders) {
        & (Join-Path $PSScriptRoot 'verify_shaders.ps1')
    }

    foreach ($currentConfiguration in $configurations) {
        if (-not $SkipEngine) {
            Invoke-Build $msbuild $engineProject $currentConfiguration
            $engineOutput = Join-Path $projectRoot "x64\$currentConfiguration"
            $engineExecutable = Join-Path $engineOutput 'DX_FrameWork.exe'
            $assimpRuntime = Join-Path $engineOutput 'assimp-vc143-mt.dll'
            if (-not (Test-Path -LiteralPath $engineExecutable)) {
                throw "Engine executable was not produced: $engineExecutable"
            }
            if (-not (Test-Path -LiteralPath $assimpRuntime)) {
                throw "Assimp runtime is missing beside the engine executable: $assimpRuntime"
            }
        }

        if (-not $SkipTests) {
            Invoke-Build $msbuild $testProject $currentConfiguration
            $testExecutable = Join-Path $projectRoot "x64\$currentConfiguration\tests\DXFrameworkCoreTests.exe"
            Write-Host "==> Run core contracts [$currentConfiguration|x64]"
            & $testExecutable
            if ($LASTEXITCODE -ne 0) {
                throw "Core contract tests failed ($currentConfiguration|x64)"
            }
        }
    }
}
finally {
    Pop-Location
}

Write-Host 'Engine verification completed successfully.' -ForegroundColor Green
