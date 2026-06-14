param(
    [string]$OutputDir = ".testbin"
)

$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force $OutputDir | Out-Null

$specialPackages = @("bhte/mldsa", "bhte/test")
$packages = go list ./... | Where-Object { $specialPackages -notcontains $_ }

foreach ($package in $packages) {
    go test $package
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

go test -o (Join-Path $OutputDir "mldsa.test.exe") ./mldsa
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

go test -o (Join-Path $OutputDir "integration.test.exe") ./test
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "BHTE tests completed successfully."
