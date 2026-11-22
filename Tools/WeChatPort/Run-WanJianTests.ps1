<#
.SYNOPSIS
    Builds the editor target and runs the 万剑仙宗 automation tests headlessly. Prints a short result summary.

.PARAMETER SkipBuild
    Run the tests against the binaries already built.
#>
param(
    [string]$Filter = 'BoundlessImmortality.WanJian',
    [string]$EngineDir = 'F:\UNREAL\UE_5.8\UE_5.8',
    [string]$LogFile = (Join-Path $env:TEMP 'wanjian-tests.log'),
    [switch]$SkipBuild
)

$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\BoundlessImmortality.uproject')).Path

if (-not $SkipBuild) {
    $build = & "$EngineDir\Engine\Build\BatchFiles\Build.bat" BoundlessImmortalityEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoXGE -MaxParallelActions=6 2>&1
    if ($LASTEXITCODE -ne 0) {
        $build | Where-Object { $_ -match 'error|Error' } | Select-Object -First 30
        Write-Output "BUILD FAILED (exit $LASTEXITCODE)"
        exit 1
    }
    Write-Output 'build ok'
}

& "$EngineDir\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project "-ExecCmds=Automation RunTests $Filter" '-TestExit=Automation Test Queue Empty' -unattended -nullrhi -nosplash -noxgecontroller -nopause -nosound "-abslog=$LogFile" 2>&1 | Out-Null

function Get-Clean([string]$Pattern) {
    Select-String -LiteralPath $LogFile -Pattern $Pattern | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' -replace 'LogAutomationController: (Display: |Error: )?', '').Trim() }
}

$results = @(Get-Clean 'Test Completed\. Result=')
$failed = @($results | Where-Object { $_ -match 'Result=\{Fail\}' })
Get-Clean 'margin: wins from|beaten \d+ of'
Get-Clean 'Error: (Expected|Ensure)' | Select-Object -First 25 | ForEach-Object { $_.Substring(0, [Math]::Min(170, $_.Length)) }
Write-Output "tests: $($results.Count), failed: $($failed.Count)"
$failed
if ($results.Count -eq 0 -or $failed.Count -gt 0) { exit 1 }
