@echo off
chcp 65001 >nul
echo ============================================
echo  修复 MSYS2
echo ============================================
echo.

set "MSYS2_DIR=C:\msys64"

echo [1/3] 检查 MSYS2 目录...
if not exist "%MSYS2_DIR%\var\lib\pacman" (
    echo 创建 pacman 目录...
    mkdir "%MSYS2_DIR%\var\lib\pacman" 2>nul
)

echo.
echo [2/3] 初始化 pacman 密钥...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman-key --init"
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman-key --populate msys2"

echo.
echo [3/3] 更新 pacman 本身...
%MSYS2_DIR%\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"

echo.
echo ============================================
echo  修复完成！
echo ============================================
echo.
pause
