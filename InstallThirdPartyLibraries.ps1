# Clones and bootstraps vcpkg into build\third-party\vcpkg.
#
# Dependencies are declared in vcpkg.json (manifest mode) and are built
# automatically during the first MSBuild of the solution. To install them
# ahead of time, run:
#   build\third-party\vcpkg\vcpkg.exe install --triplet x64-windows

$ErrorActionPreference = 'Stop'

# Keep in sync with "builtin-baseline" in vcpkg.json.
$vcpkgCommit = '07425ae85e730721ba2deeb55937be1498a7115a'

$scriptFolder = Split-Path $script:MyInvocation.MyCommand.Path
$vcpkgFolder = Join-Path $scriptFolder 'build\third-party\vcpkg'

if (-not (Test-Path $vcpkgFolder)) {
    New-Item -ItemType Directory -Force (Split-Path $vcpkgFolder) | Out-Null
    git clone https://github.com/microsoft/vcpkg.git $vcpkgFolder
}

git -C $vcpkgFolder fetch origin $vcpkgCommit
git -C $vcpkgFolder checkout $vcpkgCommit

if (-not (Test-Path (Join-Path $vcpkgFolder 'vcpkg.exe'))) {
    & (Join-Path $vcpkgFolder 'bootstrap-vcpkg.bat') -disableMetrics
}

Write-Host 'vcpkg is ready.'
