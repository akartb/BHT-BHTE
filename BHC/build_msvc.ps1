# BHC MSVC PowerShell 编译脚本
# 使用 Visual Studio 2026 (安装在 D:\11)

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "BHC MSVC 编译脚本" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# 加载 VS 环境
$vsEnvScript = "D:\11\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vsEnvScript) {
    Write-Host "[1/5] 加载 VS 2026 环境..." -ForegroundColor Yellow
    & cmd /c "`"$vsEnvScript`" > nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^(\w+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    Write-Host "    [OK] VS 环境已加载" -ForegroundColor Green
} else {
    Write-Host "    [错误] 未找到 vcvars64.bat" -ForegroundColor Red
    exit 1
}

# 验证编译器
Write-Host "[2/5] 验证编译器..." -ForegroundColor Yellow
$clPath = where.exe cl.exe 2>$null
if ($LASTEXITCODE -ne 0 -or -not $clPath) {
    Write-Host "    [错误] cl.exe 未找到" -ForegroundColor Red
    exit 1
}
Write-Host "    [OK] 编译器路径: $clPath" -ForegroundColor Green

# 创建输出目录
$buildDir = "d:\work0\BHC\build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}
Write-Host "[3/5] 编译 BHC 核心模块..." -ForegroundColor Yellow

# 编译 ProgPoW 和 ML-DSA
$cryptoSrc = @(
    "d:\work0\BHC\src\crypto\progpow.cpp",
    "d:\work0\BHC\src\crypto\mldsa.cpp"
)

foreach ($src in $cryptoSrc) {
    $objFile = Join-Path $buildDir ((Split-Path $src -Leaf) -replace '\.cpp$', '.obj')
    Write-Host "    编译: $(Split-Path $src -Leaf)..." -NoNewline
    $output = & cl.exe /std:c++20 /O2 /EHsc /c /I"d:\work0\BHC\src" /Fo"$objFile" $src 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host " [OK]" -ForegroundColor Green
    } else {
        Write-Host " [失败]" -ForegroundColor Red
        Write-Host $output
        exit 1
    }
}

# 编译核心模块
$coreSrc = @(
    "d:\work0\BHC\src\pow.cpp",
    "d:\work0\BHC\src\chainparams.cpp",
    "d:\work0\BHC\src\script\hybrid_sig.cpp"
)

foreach ($src in $coreSrc) {
    $objFile = Join-Path $buildDir ((Split-Path $src -Leaf) -replace '\.cpp$', '.obj')
    Write-Host "    编译: $(Split-Path $src -Leaf)..." -NoNewline
    $output = & cl.exe /std:c++20 /O2 /EHsc /c /I"d:\work0\BHC\src" /Fo"$objFile" $src 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host " [OK]" -ForegroundColor Green
    } else {
        Write-Host " [失败]" -ForegroundColor Red
        Write-Host $output
        exit 1
    }
}

# 创建静态库
Write-Host "[4/5] 创建静态库..." -ForegroundColor Yellow
$cryptoObjs = (Join-Path $buildDir "progpow.obj"), (Join-Path $buildDir "mldsa.obj")
$coreObjs = (Join-Path $buildDir "pow.obj"), (Join-Path $buildDir "chainparams.obj"), (Join-Path $buildDir "hybrid_sig.obj")

& lib.exe /OUT:(Join-Path $buildDir "bhc_crypto.lib") $cryptoObjs 2>&1 | Out-Null
& lib.exe /OUT:(Join-Path $buildDir "bhc_core.lib") $coreObjs 2>&1 | Out-Null

Write-Host "    [OK] 静态库已创建" -ForegroundColor Green

# 编译独立测试程序
Write-Host "[5/5] 编译测试程序..." -ForegroundColor Yellow
$testOutput = Join-Path $buildDir "bhc_test.exe"
$output = & cl.exe /std:c++20 /O2 /EHsc /I"d:\work0\BHC\src" /Fe:$testOutput "d:\work0\BHC\tools\standalone_test.cpp" (Join-Path $buildDir "bhc_crypto.lib") 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "    [OK] 测试程序已编译" -ForegroundColor Green
} else {
    Write-Host "    [警告] 链接时出现问题，尝试直接编译..." -ForegroundColor Yellow
    # 尝试直接编译（不使用预编译库）
    $allObjs = Get-ChildItem (Join-Path $buildDir "*.obj") | ForEach-Object { $_.FullName }
    $output = & cl.exe /std:c++20 /O2 /EHsc /I"d:\work0\BHC\src" /Fe:$testOutput "d:\work0\BHC\tools\standalone_test.cpp" $allObjs 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    [OK] 测试程序已编译 (直接链接)" -ForegroundColor Green
    } else {
        Write-Host "    [错误] 测试程序编译失败" -ForegroundColor Red
        Write-Host $output
        exit 1
    }
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "编译完成!" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "输出文件:" -ForegroundColor Yellow
Get-ChildItem $buildDir | ForEach-Object { Write-Host "  - $($_.Name)" }
Write-Host ""

# 运行测试
Write-Host "运行测试程序..." -ForegroundColor Yellow
& (Join-Path $buildDir "bhc_test.exe")

Write-Host ""
Write-Host "按任意键退出..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
