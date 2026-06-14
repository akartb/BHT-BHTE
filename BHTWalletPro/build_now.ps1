$Qt6_DIR = "D:\Qt\6.5.0\msvc2019_64\lib\cmake\Qt6"
$BuildDir = "D:\work0\BHT\BHTWalletPro\build"
$SourceDir = "D:\work0\BHT\BHTWalletPro"

Write-Host "=== BHT Wallet Pro Build Script ===" -ForegroundColor Cyan
Write-Host "Qt6: $Qt6_DIR" -ForegroundColor Yellow

if (Test-Path $BuildDir) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

Write-Host "Running CMake..." -ForegroundColor Yellow

cmake -S $SourceDir -B $BuildDir `
    -G "NMake Makefiles" `
    -DCMAKE_PREFIX_PATH="D:\Qt\6.5.0\msvc2019_64" `
    -DQt6_DIR="$Qt6_DIR" `
    -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration FAILED!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Building..." -ForegroundColor Yellow

cmake --build $BuildDir --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Build SUCCESS ===" -ForegroundColor Green

$ExePath = Join-Path $BuildDir "bin" "BHT-Wallet.exe"
if (Test-Path $ExePath) {
    Write-Host "Output: $ExePath" -ForegroundColor Green
} else {
    Write-Host "WARNING: Expected output not found at $ExePath" -ForegroundColor Yellow
    Get-ChildItem -Path $BuildDir -Recurse -Filter "*.exe" | Select-Object FullName
}
