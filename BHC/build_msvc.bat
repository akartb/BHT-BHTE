@echo off
REM BHC MSVC 编译脚本
REM 使用 Visual Studio 2026 (安装在 D:\11)

echo ============================================================
echo BHC MSVC 编译脚本
echo ============================================================
echo.

REM 设置 VS 环境
call "D:\11\VC\Auxiliary\Build\vcvars64.bat"

echo [OK] VS 环境已加载
echo 编译器路径:
where cl.exe
echo.

REM 编译 BHC 核心模块
echo [1/2] 编译 BHC 核心库...
cl.exe /std:c++20 /O2 /EHsc /c /I"src" /Fo"build\bhc_crypto.obj" src\crypto\progpow.cpp src\crypto\mldsa.cpp
if %errorlevel% neq 0 (
    echo [错误] 核心库编译失败
    pause
    exit /b 1
)

cl.exe /std:c++20 /O2 /EHsc /c /I"src" /Fo"build\bhc_core.obj" src\pow.cpp src\chainparams.cpp src\script\hybrid_sig.cpp
if %errorlevel% neq 0 (
    echo [错误] 核心模块编译失败
    pause
    exit /b 1
)

echo.
echo [2/2] 链接生成可执行文件...
lib.exe /OUT:"build\bhc_crypto.lib" "build\bhc_crypto.obj"
lib.exe /OUT:"build\bhc_core.lib" "build\bhc_core.obj"

cl.exe /std:c++20 /O2 /EHsc /Fe:"build\bhc_standalone.exe" tools\standalone_test.cpp /Fo"build\standalone.obj"
if %errorlevel% neq 0 (
    echo [错误] 可执行文件生成失败
    pause
    exit /b 1
)

echo.
echo ============================================================
echo 编译完成!
echo ============================================================
echo.
echo 二进制文件: build\bhc_standalone.exe
echo.

build\bhc_standalone.exe

pause
