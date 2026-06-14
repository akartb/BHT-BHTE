@echo off
chcp 65001 >nul
echo ============================================
echo  安装 Qt6 和编译工具
echo ============================================
echo.

set "MSYS2_DIR=C:\msys64"

echo [INFO] 启动 MSYS2 MinGW64 终端进行安装...
echo.

REM 创建临时安装脚本
echo #!/bin/bash > "%TEMP%\install_qt6.sh"
echo echo "========================================" >> "%TEMP%\install_qt6.sh"
echo echo "  在 MSYS2 中安装 Qt6" >> "%TEMP%\install_qt6.sh"
echo echo "========================================" >> "%TEMP%\install_qt6.sh"
echo echo "" >> "%TEMP%\install_qt6.sh"
echo echo "[1/3] 更新包数据库..." >> "%TEMP%\install_qt6.sh"
echo pacman -Sy >> "%TEMP%\install_qt6.sh"
echo echo "" >> "%TEMP%\install_qt6.sh"
echo echo "[2/3] 安装 Qt6 基础组件 (需要几分钟)..." >> "%TEMP%\install_qt6.sh"
echo pacman -S --noconfirm mingw-w64-x86_64-qt6-base >> "%TEMP%\install_qt6.sh"
echo echo "" >> "%TEMP%\install_qt6.sh"
echo echo "[3/3] 安装 CMake 和 GCC..." >> "%TEMP%\install_qt6.sh"
echo pacman -S --noconfirm mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-make >> "%TEMP%\install_qt6.sh"
echo echo "" >> "%TEMP%\install_qt6.sh"
echo echo "安装完成！按任意键关闭..." >> "%TEMP%\install_qt6.sh"
echo read -n 1 >> "%TEMP%\install_qt6.sh"

REM 启动 MSYS2 MinGW64 终端并执行脚本
start "" "%MSYS2_DIR%\mingw64.exe" -c "bash /tmp/install_qt6.sh"

echo.
echo MSYS2 终端已启动，请在弹出的窗口中完成安装
echo.
pause
