param(
    [string]$OutputDir = ".testbin"
)

$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force $OutputDir | Out-Null

# Windows can deny executing Go test binaries from the deep temp build path on
# this machine ("Access is denied"), so every test package is compiled to a
# stable project path with `go test -c` and then run from there.

$packages = go list ./...

$failed = $false
foreach ($package in $packages) {
    $testCounts = go list -f '{{len .TestGoFiles}} {{len .XTestGoFiles}}' $package
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
        continue
    }

    if ($testCounts -eq "0 0") {
        continue
    }

    # Derive a safe binary name from the import path.
    $name = ($package -replace '[\\/]', '_') + ".test.exe"
    $outPath = Join-Path $OutputDir $name

    go test -c -o $outPath $package
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
        continue
    }

    & $outPath "-test.timeout=10m"
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
    }
}

if ($failed) {
    Write-Host "BHTE tests FAILED."
    exit 1
}

Write-Host "BHTE tests completed successfully."
