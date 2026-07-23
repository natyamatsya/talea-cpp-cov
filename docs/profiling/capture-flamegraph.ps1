<#
.SYNOPSIS
    ETW CPU-sampling capture of an OpenCppCoverage run, for flame graphs.

.DESCRIPTION
    Records a system-wide CPU sampling trace (PROC_THREAD+LOADER+PROFILE with
    stackwalk, 8 kHz) while OpenCppCoverage collects coverage of a target, then
    writes a merged .etl. ETW kernel sampling needs elevation, so the script
    re-launches itself elevated (one UAC prompt) if it is not already admin.

    Fold the result into a flame graph (see README.md):
      $env:_NT_SYMBOL_PATH = '<bin-with-pdbs>'
      xperf -i out.etl -symbols -a dumper -stacktimeshifting `
        | python fold-flamegraph.py OpenCppCoverage.exe out.svg

.EXAMPLE
    .\capture-flamegraph.ps1 -OccExe out\clang-cl-profile\bin\OpenCppCoverage.exe `
        -Target out\clang-cl-profile\bin\CppCoverageTest.exe `
        -Sources . -Modules CppCoverageTest -Etl occ.etl
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OccExe,
    [Parameter(Mandatory)][string]$Target,
    [string]$Sources = "*",
    [string]$Modules = "*",
    [string]$TargetArgs = "",          # space-separated args after `--`
    [switch]$NoVeh,                     # add --no_veh (VEH is the default engine)
    [string]$Etl = "occ.etl",
    [int]$ProfIntervalUnits = 1221      # ~8 kHz (0.1 us units)
)
$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    $a = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$PSCommandPath`"",
           '-OccExe',"`"$OccExe`"", '-Target',"`"$Target`"", '-Sources',"`"$Sources`"",
           '-Modules',"`"$Modules`"", '-TargetArgs',"`"$TargetArgs`"", '-Etl',"`"$Etl`"",
           '-ProfIntervalUnits',$ProfIntervalUnits)
    if ($NoVeh) { $a += '-NoVeh' }
    Write-Host "Not elevated - relaunching with admin rights (approve the UAC prompt)..."
    Start-Process powershell -Verb RunAs -Wait -ArgumentList $a
    return
}

function Find-Xperf {
    $c = Get-Command xperf.exe -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $p = "${env:ProgramFiles(x86)}\Windows Kits\10\Windows Performance Toolkit\xperf.exe"
    if (Test-Path $p) { return $p }
    throw "xperf.exe not found (install the Windows Performance Toolkit / Windows SDK)."
}
$xperf = Find-Xperf
$cov = [System.IO.Path]::ChangeExtension($Etl, ".cov")
$occArgs = @('--quiet')
if ($NoVeh) { $occArgs += '--no_veh' }
$occArgs += @('--sources', $Sources, '--modules', $Modules, '--export_type', "binary:$cov", '--', $Target)
if ($TargetArgs) { $occArgs += ($TargetArgs -split ' ') }

Write-Host "Capturing to $Etl (engine: $(if($NoVeh){'debugger'}else{'VEH'}))..."
& $xperf -setprofint $ProfIntervalUnits | Out-Null
& $xperf -on PROC_THREAD+LOADER+PROFILE -stackwalk Profile | Out-Null
try {
    & $OccExe @occArgs 2>&1 | Out-Null
} finally {
    & $xperf -stop -d $Etl | Out-Null
}
Remove-Item $cov -ErrorAction SilentlyContinue
Write-Host "Wrote $Etl"
