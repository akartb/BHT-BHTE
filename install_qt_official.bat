@echo off
echo ============================================
echo  Qt Official Installer
echo ============================================
echo.

set QT_INSTALL_DIR=D:\Qt6

echo [INFO] Qt will be installed to: %QT_INSTALL_DIR%
echo.

echo [1/2] Downloading Qt Online Installer...
echo This may take a few minutes...

if not exist "%TEMP%\qt-installer.exe" (
    powershell -Command "Invoke-WebRequest -Uri 'https://download.qt.io/official_releases/online_installers/qt-unified-windows-x64-4.6.0-online.exe' -OutFile '%TEMP%\qt-installer.exe'"
)

echo.
echo [2/2] Launching Qt Installer...
echo.
echo Please follow the steps:
echo   1. Login or create Qt account
echo   2. Select "Custom Installation"
echo   3. Select Qt 6.x.x - MinGW version
echo   4. Set install path: D:\Qt6
echo   5. Start installation
echo.

start "" "%TEMP%\qt-installer.exe"

echo After installation completes, press any key to continue...
pause
