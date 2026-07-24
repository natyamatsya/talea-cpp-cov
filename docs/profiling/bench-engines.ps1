<#
.SYNOPSIS
    Best-of-N wall-clock of covering the same target with different engines/tools.

.DESCRIPTION
    Times three configurations on identical inputs: an optional baseline
    talea-cpp-cov (e.g. the official release), this fork's classic debugger
    engine (--engine debugger), and this fork's default in-process VEH engine. Reports
    the best of N runs and the speedups. No elevation required.

.EXAMPLE
    .\bench-engines.ps1 -ForkExe out\clang-cl-release\bin\talea-cpp-cov.exe `
        -Target out\clang-cl-release\bin\TaleaCppCovTest.exe `
        -Sources . -Modules TaleaCppCovTest -BaselineExe C:\official\talea-cpp-cov.exe
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ForkExe,
    [Parameter(Mandatory)][string]$Target,
    [string]$Sources = "*",
    [string]$Modules = "*",
    [string]$TargetArgs = "",
    [string]$BaselineExe = "",   # optional: e.g. the official release
    [int]$N = 5
)
$ErrorActionPreference = "Stop"
$out = Join-Path $env:TEMP "talea_bench.cov"
$ta = if ($TargetArgs) { $TargetArgs -split ' ' } else { @() }

function Best([string]$talea, [string[]]$extra) {
    $best = [double]::MaxValue
    for ($i = 0; $i -lt $N; $i++) {
        Remove-Item $out -ErrorAction SilentlyContinue
        $ms = (Measure-Command {
            & $talea --quiet @extra --sources $Sources --modules $Modules `
                --export_type "binary:$out" -- $Target @ta 2>&1 | Out-Null
        }).TotalMilliseconds
        if ($ms -lt $best) { $best = $ms }
    }
    [math]::Round($best)
}

$results = [ordered]@{}
if ($BaselineExe) { $results["baseline (debugger)"] = Best $BaselineExe @() }
$results["fork (debugger)"] = Best $ForkExe @('--engine', 'debugger')
$results["fork (veh)"]      = Best $ForkExe @()

"" ; "Best of $N runs (ms):"
$results.GetEnumerator() | ForEach-Object { "  {0,-22} {1,6}" -f $_.Key, $_.Value }
if ($BaselineExe) {
    $b = $results["baseline (debugger)"]
    "" ; "Speedup vs baseline:"
    "  fork (debugger)  {0:N1}x" -f ($b / $results["fork (debugger)"])
    "  fork (veh)        {0:N1}x" -f ($b / $results["fork (veh)"])
}
