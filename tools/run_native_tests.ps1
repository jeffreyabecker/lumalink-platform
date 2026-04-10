$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

Push-Location $repoRoot

try {
    cmake -S . -B $buildDir
    cmake --build $buildDir --config Debug
    ctest --test-dir $buildDir --config Debug --output-on-failure
}
finally {
    Pop-Location
}