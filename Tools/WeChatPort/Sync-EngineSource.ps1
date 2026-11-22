<#
.SYNOPSIS
    Materialises the pinned UE WebGPU fork commit on a connection that keeps resetting.

.DESCRIPTION
    A single `git sparse-checkout disable` asks GitHub for every missing blob in one pack, so one reset
    loses everything downloaded so far. This script fetches the missing blobs of the pinned commit in small
    batches and keeps each finished batch. It rotates between network routes when one stops working, grows
    the batch while the link is healthy and shrinks it after failures, and only disables the sparse checkout
    when no blob is missing, so that step needs no network.

    Safe to re-run: every start begins from what is still missing.
#>
param(
    [string]$EngineDir = 'G:\CodexUEWasm\EngineSource',
    [string]$LogDir = 'G:\CodexUEWasm\logs',
    [string]$PinnedCommit = '51fb9a63563a7f5c6d87aa470d3a3d5847cb7e72',
    [int]$MinBatch = 40,
    [int]$MaxBatch = 4000,
    [double]$PatienceHours = 36,
    [switch]$SkipCheckout
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $LogDir | Out-Null
$logFile = Join-Path $LogDir 'sync-engine-source.log'
$batchFile = Join-Path $LogDir 'sync-batch.txt'

# Each route is a set of `git -c` options. `http.proxy=` empties the proxy from the global git config for one fetch.
$routes = @(
    @{ Name = 'proxy';   Options = '' },
    @{ Name = 'direct';  Options = '-c http.proxy=' },
    @{ Name = 'direct2'; Options = '-c http.proxy= -c http.curloptResolve=github.com:443:20.27.177.113' },
    @{ Name = 'direct3'; Options = '-c http.proxy= -c http.curloptResolve=github.com:443:20.200.245.247' }
)

function Write-Log([string]$Message) {
    $line = '{0:yyyy-MM-dd HH:mm:ss}  {1}' -f (Get-Date), $Message
    Add-Content -LiteralPath $logFile -Value $line -Encoding utf8
    [Console]::Out.WriteLine($line)
}

function Get-MissingBlobs {
    $objects = & git -C $EngineDir rev-list --objects --missing=print --no-walk HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git rev-list failed' }
    return @($objects | Where-Object { $_.StartsWith('?') } | ForEach-Object { $_.Substring(1) })
}

function Get-PackGiB {
    return (Get-ChildItem -LiteralPath (Join-Path $EngineDir '.git\objects\pack') -File | Measure-Object Length -Sum).Sum / 1GB
}

function Invoke-BatchFetch([string[]]$Oids, [hashtable]$Route) {
    # git fetch --stdin keeps a trailing CR as part of the refspec, so write bare LF line endings.
    [IO.File]::WriteAllText($batchFile, (($Oids -join "`n") + "`n"))
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # Same command git runs for a lazy promisor fetch, plus stall detection so a dead link fails quickly.
        $cmd = "git -C `"$EngineDir`" $($Route.Options) -c fetch.negotiationAlgorithm=noop -c http.version=HTTP/1.1 " +
            "-c http.lowSpeedLimit=2000 -c http.lowSpeedTime=45 fetch origin --no-tags --no-write-fetch-head " +
            "--recurse-submodules=no --filter=blob:none --stdin < `"$batchFile`" 2>&1"
        $output = & cmd.exe /d /c $cmd
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            $tail = ($output | Select-Object -Last 1) -join ' '
            Write-Log "  [$($Route.Name)] $($Oids.Count) objects failed: $tail"
        }
        return ($code -eq 0)
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

$head = (& git -C $EngineDir rev-parse HEAD).Trim()
if ($head -ne $PinnedCommit) { throw "HEAD is $head, expected the pinned commit $PinnedCommit" }
Write-Log "HEAD verified at $head"

$deadline = (Get-Date).AddHours($PatienceHours)
$batch = 300
$routeIndex = 0
$sinceSuccess = 0
$lastReport = Get-Date

$missing = Get-MissingBlobs
$total = $missing.Count
Write-Log "$total blobs missing"
$offset = 0

while ($true) {
    if ($offset -ge $missing.Count) {
        $missing = Get-MissingBlobs
        Write-Log "rescan: $($missing.Count) blobs missing"
        if ($missing.Count -eq 0) { break }
        $total = $missing.Count
        $offset = 0
    }

    if ((Get-Date) -gt $deadline) {
        Write-Log "patience exhausted with $($missing.Count - $offset) blobs left in this scan; re-run this script"
        exit 2
    }

    $count = [Math]::Min($batch, $missing.Count - $offset)
    [string[]]$oids = $missing[$offset..($offset + $count - 1)]
    $route = $routes[$routeIndex]

    if (Invoke-BatchFetch $oids $route) {
        $offset += $count
        $sinceSuccess = 0
        $batch = [Math]::Min($MaxBatch, [int]($batch * 1.5) + 1)
        if (((Get-Date) - $lastReport).TotalSeconds -ge 20) {
            $lastReport = Get-Date
            Write-Log ('  [{0}] {1}/{2} blobs, packs {3:N2} GiB, batch {4}' -f $route.Name, $offset, $total, (Get-PackGiB), $batch)
        }
        continue
    }

    $sinceSuccess++
    $batch = [Math]::Max($MinBatch, [int]($batch / 2))
    $routeIndex = ($routeIndex + 1) % $routes.Count
    # Quick rotation first; after a full round of failures back off so a dead network is not hammered.
    if ($sinceSuccess -ge $routes.Count) {
        Start-Sleep -Seconds ([Math]::Min(120, 10 * ($sinceSuccess - $routes.Count + 1)))
    }
}

Write-Log 'all blobs present'
if (-not $SkipCheckout) {
    Write-Log 'disabling sparse checkout (materialises the working tree)'
    & git -C $EngineDir sparse-checkout disable
    if ($LASTEXITCODE -ne 0) { throw 'git sparse-checkout disable failed' }
    Write-Log 'working tree materialised'
}
