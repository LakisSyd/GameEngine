[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$windowsKitsBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$fxc = Get-ChildItem -LiteralPath $windowsKitsBin -Filter fxc.exe -Recurse -File `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.Directory.Name -eq 'x64' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1

if (-not $fxc) {
    throw 'fxc.exe was not found. Install a Windows 10/11 SDK with the HLSL compiler.'
}

$shaderChecks = @(
    @('Shader/Vs/texture.vs',             'TextureVertexShader',      'vs_5_0'),
    @('Shader/Ps/texture.ps',             'TexturePixelShader',       'ps_5_0'),
    @('Shader/Vs/Multitexture.vs',        'MultiTextureVertexShader', 'vs_5_0'),
    @('Shader/Ps/Multitexture.ps',        'MultiTexturePixelShader',  'ps_5_0'),
    @('Shader/Vs/Lightmap.vs',            'LightMapVertexShader',     'vs_5_0'),
    @('Shader/Ps/Lightmap.ps',            'LightMapPixelShader',      'ps_5_0'),
    @('Shader/Vs/Normalmap.vs',           'NormalMapVertexShader',    'vs_5_0'),
    @('Shader/Ps/Normalmap.ps',           'NormalMapPixelShader',     'ps_5_0'),
    @('Shader/Vs/fog.vs',                 'FogVertexShader',          'vs_5_0'),
    @('Shader/Ps/fog.ps',                 'FogPixelShader',           'ps_5_0'),
    @('Shader/Vs/clipplane.vs',           'ClipPlaneVertexShader',    'vs_5_0'),
    @('Shader/Ps/clipplane.ps',           'ClipPlanePixelShader',     'ps_5_0'),
    @('Shader/Vs/color.vs',               'ColorVertexShader',        'vs_5_0'),
    @('Shader/Ps/color.ps',               'ColorPixelShader',         'ps_5_0'),
    @('Shader/Vs/projection.vs',          'ProjectionVertexShader',   'vs_5_0'),
    @('Shader/Ps/projection.ps',          'ProjectionPixelShader',    'ps_5_0'),
    @('Shader/Vs/Water.vs',               'WaterVertexShader',        'vs_5_0'),
    @('Shader/Ps/Water.ps',               'WaterPixelShader',         'ps_5_0'),
    @('Shader/Vs/glass.vs',               'GlassVertexShader',        'vs_5_0'),
    @('Shader/Ps/glass.ps',               'GlassPixelShader',         'ps_5_0'),
    @('Shader/Vs/reflection.vs',          'ReflectionVertexShader',   'vs_5_0'),
    @('Shader/Ps/reflection.ps',          'ReflectionPixelShader',    'ps_5_0'),
    @('Shader/Vs/light.vs',               'LightVertexShader',        'vs_5_0'),
    @('Shader/Ps/light.ps',               'LightPixelShader',         'ps_5_0'),
    @('Shader/Vs/pbr.vs',                 'PBRVertexShader',          'vs_5_0'),
    @('Shader/Ps/pbr.ps',                 'PBRPixelShader',           'ps_5_0'),
    @('Shader/Vs/TerrainBlend.vs',        'TerrainBlendVertexShader', 'vs_5_0'),
    @('Shader/Ps/TerrainBlend.ps',        'TerrainBlendPixelShader',  'ps_5_0'),
    @('Shader/Vs/SkyShader.vs',           'SkyVS',                    'vs_5_0'),
    @('Shader/Ps/SkyShader.ps',           'SkyPS',                    'ps_5_0'),
    @('Shader/Vs/shadow.vs',              'ShadowVertexShader',       'vs_5_0'),
    @('Shader/Ps/shadow.ps',              'ShadowPixelShader',        'ps_5_0'),
    @('Shader/Vs/particle.vs',            'ParticleVertexShader',     'vs_5_0'),
    @('Shader/Ps/particle.ps',            'ParticlePixelShader',      'ps_5_0'),
    @('Grass/grass_instanced.vs',         'GrassVertexShader',        'vs_5_0'),
    @('Grass/grass_instanced.ps',         'GrassPixelShader',         'ps_5_0')
)

Push-Location $projectRoot
try {
    foreach ($check in $shaderChecks) {
        $path = Join-Path 'src\Render' $check[0]
        $compilerOutput = & $fxc.FullName `
            /nologo /Ges /WX `
            /T $check[2] `
            /E $check[1] `
            /Fo NUL `
            $path 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Shader validation failed: $path ($($check[1]))`n$compilerOutput"
        }
    }
}
finally {
    Pop-Location
}

Write-Host "Validated $($shaderChecks.Count) shader entry points." -ForegroundColor Green
