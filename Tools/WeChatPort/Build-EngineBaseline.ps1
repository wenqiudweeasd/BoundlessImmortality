<#
.SYNOPSIS
    Builds the pinned UE WebGPU fork in stages, following the fork's build-windows.md.

.DESCRIPTION
    Stages run in order from -From to -To and stop at the first failure. Each writes its own log under -LogDir.

    Setup.bat is deliberately not used: besides downloading dependencies it runs the prerequisite installer and registers
    the engine with UnrealVersionSelector, which could disturb the installed UE 5.8.4. Only its dependency download runs here.

    Stages: deps, validate, dawn, tint, thirdparty, editor, tools, html5
#>
param(
    [string]$EngineDir = 'G:\CodexUEWasm\EngineSource',
    [string]$LogDir = 'G:\CodexUEWasm\logs',
    [string]$ToolsDir = 'D:\demo\BoundlessImmortality\Saved\Codex\UEWasmTools',
    [string]$From = 'deps',
    [string]$To = 'html5',
    [int]$Jobs = 6
)

$ErrorActionPreference = 'Stop'
$stages = 'deps', 'validate', 'dawn', 'tint', 'thirdparty', 'editor', 'tools', 'html5'
New-Item -ItemType Directory -Force $LogDir | Out-Null

# Toolchain for this process only; nothing is written to the user's environment.
$emsdk = Join-Path $ToolsDir 'emsdk'
$env:EMSDK = $emsdk
$env:EM_CONFIG = Join-Path $emsdk '.emscripten'
$env:EMSDK_PYTHON = Join-Path $emsdk 'python\3.13.3_64bit\python.exe'
$env:EMSDK_NODE = Join-Path $emsdk 'node\24.19.0_64bit\node.exe'
$env:RUSTUP_HOME = Join-Path $ToolsDir 'rustup'
$env:CARGO_HOME = Join-Path $ToolsDir 'cargo'
$vs = 'D:\VS\IDE'
$paths = @(
    (Join-Path $ToolsDir 'cargo\bin'),
    (Join-Path $emsdk 'upstream\emscripten'),
    (Split-Path $env:EMSDK_PYTHON),
    (Split-Path $env:EMSDK_NODE),
    (Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'),
    (Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja')
)
$env:PATH = ($paths -join ';') + ';' + $env:PATH
$python = $env:EMSDK_PYTHON
$runUbt = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUBT.bat'

function Invoke-Stage([string]$Name, [scriptblock]$Body) {
    $log = Join-Path $LogDir "baseline-$Name.log"
    $started = Get-Date
    Write-Output ("[{0:HH:mm:ss}] {1} ..." -f $started, $Name)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Body *>&1 | ForEach-Object { $_.ToString() } | Out-File -LiteralPath $log -Encoding utf8
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    $minutes = [Math]::Round(((Get-Date) - $started).TotalMinutes, 1)
    if ($code -ne 0) {
        Write-Output "FAILED $Name (exit $code) after $minutes min, log: $log"
        Get-Content -LiteralPath $log -Tail 25 | ForEach-Object { $_.Substring(0, [Math]::Min(220, $_.Length)) }
        exit 1
    }
    Write-Output "ok $Name in $minutes min"
}

Set-Location $EngineDir
$first = [Array]::IndexOf($stages, $From)
$last = [Array]::IndexOf($stages, $To)
if ($first -lt 0 -or $last -lt 0) { throw "Unknown stage. Valid stages: $($stages -join ', ')" }

foreach ($stage in $stages[$first..$last]) {
    switch ($stage) {
        'deps' {
            Invoke-Stage $stage {
                & (Join-Path $EngineDir 'Engine\Binaries\DotNET\GitDependencies\win-x64\GitDependencies.exe') --force --threads=8 --max-retries=20 `
                    --exclude=Mac --exclude=osx64 --exclude=IOS --exclude=TVOS --exclude=Android --exclude=Linux --exclude=Linux32 --exclude=HoloLens
            }
        }
        'validate' { Invoke-Stage $stage { & $runUbt -Mode=ValidatePlatforms -Platforms=Win64+HTML5 } }
        'dawn' { Invoke-Stage $stage { & $python 'Engine\Source\ThirdParty\Dawn\BuildForUE\FetchDawn.py' --platform win64 } }
        'tint' { Invoke-Stage $stage { & $python 'Engine\Source\ThirdParty\Dawn\BuildForUE\BuildTint.py' --jobs $Jobs } }
        'thirdparty' { Invoke-Stage $stage { & $python 'Engine\Platforms\HTML5\Build\BuildThirdParty.py' --jobs $Jobs } }
        'editor' { Invoke-Stage $stage { & $runUbt UnrealEditor Win64 Development -NoUBA -NoDebugInfo "-MaxParallelActions=$Jobs" } }
        'tools' { Invoke-Stage $stage { & $runUbt '-Target=ShaderCompileWorker Win64 Development' '-Target=UnrealPak Win64 Development' -NoUBA -NoDebugInfo "-MaxParallelActions=$Jobs" } }
        'html5' { Invoke-Stage $stage { & $runUbt UnrealGame HTML5 Development -NoUBA "-MaxParallelActions=$Jobs" '-architecture=wasm64nothreads' } }
    }
}
Write-Output 'baseline stages finished'
