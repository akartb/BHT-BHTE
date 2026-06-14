@echo off
chcp 65001 >nul
echo ============================================
echo  安装 Qt6 和编译工具
echo ============================================
echo.

REM MSYS2 路径
set "MSYS2_DIR=C:\msys64"

echo [INFO] MSYS2 路径: %MSYS2_DIR%
echo.

REM 更新包数据库 - 使用 mingw64 环境
echo [1/4] 更新 MSYS2 包数据库...
%MSYS2_DIR%\msys2.exe -mingw64 -c "pacman -Sy"
if errorlevel 1 (
    echo [错误] 更新失败，尝试直接运行...
    %MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -Sy"
)

echo.
echo [2/4] 安装 Qt6 基础组件 (约 500MB，需要几分钟)...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-qt6-base"
if errorlevel 1 (
    echo [错误] Qt6 安装失败
    pause
    exit /b 1
)

echo.
echo [3/4] 安装 CMake 和 GCC...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-make"
if errorlevel 1 (
    echo [错误] 编译工具安装失败
    pause
    exit /b 1
)

echo.
echo [4/4] 创建 Qt6 符号链接到 D 盘...
if not exist "D:\Qt6" mklink /J "D:\Qt6" "%MSYS2_DIR%\mingw64"

echo.
echo ============================================
echo  Qt6 安装完成！
echo ============================================
echo.
echo Qt6 位置: C:\msys64\mingw64
echo 符号链接: D:\Qt6
echo.
echo 现在可以编译 BHT Wallet Pro 了
echo.
pause
