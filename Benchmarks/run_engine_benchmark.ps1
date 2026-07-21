<#
.SYNOPSIS
    End-to-end engine / startup benchmark for OpenCppCoverage.

.DESCRIPTION
    Benchmarks.exe (google-benchmark) covers the in-process micro-benchmarks
    (merge, serialize, Cobertura export, disassembly). This script covers the
    whole-run numbers that google-benchmark's in-process loop cannot represent:
    the collection engines and the startup path, which require spawning a real
    target and parsing real PDBs.

    It times the same scenarios the performance work was measured against
    (docs/PERFORMANCE.md): the debugger vs. the in-process VEH engine, and the
    cold-cache startup path with and without --preparse_imports. Each scenario
    is run several times and the best (lowest-noise) wall-clock time is reported.

    The deltas depend entirely on the target, so point it at something
    representative:
      * hit-heavy (many distinct executed lines) to show --experimental_veh;
      * many / large import DLLs to show --preparse_imports;
      * a large PDB to show the cold parse.
    A trivial program will show all engines within noise of each other.

.EXAMPLE
    .\run_engine_benchmark.ps1 -Target C:\path\to\your_app.exe `
        -Sources "your_src*" -Modules "your_app"
#>
[CmdletBinding()]
param(
    [string]$OpenCppCoverage = "$PSScriptRoot\..\x64\Release\OpenCppCoverage.exe",
    [Parameter(Mandatory = $true)][string]$Target,
    [string]$Sources = "*",
    [string]$Modules = "*",
    [int]$Iterations = 5,
    [string[]]$TargetArgs = @()
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $OpenCppCoverage)) { throw "OpenCppCoverage not found: $OpenCppCoverage" }
if (-not (Test-Path $Target)) { throw "Target not found: $Target" }

$cache = Join-Path $env:LOCALAPPDATA "OpenCppCoverage\PdbCache"
$output = [System.IO.Path]::GetTempFileName()

function Clear-PdbCache {
    if (Test-Path $cache) { Remove-Item $cache -Recurse -Force -ErrorAction SilentlyContinue }
}

function Invoke-Run([string[]]$flags) {
    # The target's own exit code is irrelevant to the measurement.
    & $OpenCppCoverage --quiet --sources $Sources --modules $Modules @flags `
        --export_type "binary:$output" -- $Target @TargetArgs 2>&1 | Out-Null
}

function Measure-Scenario([string]$name, [string[]]$flags, [bool]$cold) {
    $best = [double]::MaxValue
    for ($i = 0; $i -lt $Iterations; $i++) {
        if ($cold) { Clear-PdbCache }
        $ms = (Measure-Command { Invoke-Run $flags }).TotalMilliseconds
        if ($ms -lt $best) { $best = $ms }
    }
    [pscustomobject]@{
        Scenario  = $name
        Cache     = if ($cold) { "cold" } else { "warm" }
        "Best ms" = [math]::Round($best)
    }
}

# Warm the cache once so the "warm" scenarios start from a populated cache.
Clear-PdbCache
Invoke-Run @()

$results = @(
    Measure-Scenario "debugger"                @()                     $false
    Measure-Scenario "experimental_veh"        @("--experimental_veh") $false
    Measure-Scenario "debugger"                @()                     $true
    Measure-Scenario "debugger + preparse"     @("--preparse_imports") $true
)

Write-Host ""
Write-Host "Target: $Target  (best of $Iterations runs)"
$results | Format-Table -AutoSize

Remove-Item $output -Force -ErrorAction SilentlyContinue
