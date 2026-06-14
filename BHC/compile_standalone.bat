@echo off
REM BHC 独立测试编译脚本
REM 不需要 CMake，直接使用编译器

echo ============================================================
echo BHC 独立测试编译
echo ============================================================
echo.

REM 检测编译器
set COMPILER=
where g++ >nul 2>&1
if %errorlevel% equ 0 (
    set COMPILER=g++
    echo [OK] 检测到 g++
    goto :compile
)

where clang++ >nul 2>&1
if %errorlevel% equ 0 (
    set COMPILER=clang++
    echo [OK] 检测到 clang++
    goto :compile
)

where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    set COMPILER=cl
    echo [OK] 检测到 MSVC cl.exe
    goto :compile_msvc
)

echo [错误] 未检测到 C++ 编译器
echo 请安装以下任一编译器:
echo   - MinGW-w64: https://www.mingw-w64.org/
echo   - LLVM Clang: https://releases.llvm.org/
echo   - Visual Studio: https://visualstudio.microsoft.com/
pause
exit /b 1

:compile
echo.
echo 编译中...
%COMPILER% -std=c++20 -O2 -o standalone_test.exe tools\standalone_test.cpp
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)
goto :run

:compile_msvc
echo.
echo 编译中...
cl.exe /std:c++20 /O2 /EHsc /Fe:standalone_test.exe tools\standalone_test.cpp
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

:run
echo.
echo ============================================================
echo 编译成功! 运行测试...
echo ============================================================
echo.
standalone_test.exe

pause
