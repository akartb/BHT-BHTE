@echo off
echo ============================================
echo  Qt Minimal Installer (D Drive)
echo ============================================
echo.

set QT_INSTALL_DIR=D:\Qt6

echo [INFO] Installing minimal Qt6 to: %QT_INSTALL_DIR%
echo.

REM Check if Qt is already installed
if exist "%QT_INSTALL_DIR%\bin\qmake.exe" (
    echo Qt6 is already installed!
    goto :done
)

REM Create Qt6 directory
if not exist "%QT_INSTALL_DIR%" mkdir "%QT_INSTALL_DIR%"

echo [1/3] Downloading Qt6 Minimal Package...
echo.
echo Downloading from: https://github.com/qt/qtbase/releases
echo.

REM Download Qt6 base using curl (if available)
if exist "%SYSTEMROOT%\System32\curl.exe" (
    echo Using curl to download...
    curl -L -o "%TEMP%\qt6_minimal.7z" "https://github.com/qt/qtbase/archive/refs/tags/v6.5.3.tar.gz" 2>nul
)

echo.
echo [2/3] Alternative: Manual Download Required
echo.
echo Please download Qt6 manually from:
echo   https://download.qt.io/archive/qt/6.5/6.5.3/
echo.
echo Download: qt-base-windows-x86_64-mingw.7z
echo Extract to: D:\Qt6
echo.

:start_manual
set /p manual="Have you manually installed Qt6 to D:\Qt6? (y/n): "
if /i "%manual%"=="y" goto :verify
if /i "%manual%"=="n" goto :manual_install

goto :start_manual

:manual_install
echo.
echo Opening Qt download page in browser...
start "" "https://download.qt.io/archive/qt/6.5/6.5.3/"
echo.
echo After downloading and extracting Qt6 to D:\Qt6,
echo press any key to continue...
pause

:verify
echo.
echo [3/3] Verifying Qt6 installation...
if exist "%QT_INSTALL_DIR%\bin\qmake.exe" (
    echo.
    echo ============================================
    echo  Qt6 installation verified!
    echo ============================================
    echo.
    "%QT_INSTALL_DIR%\bin\qmake.exe" --version
    goto :done
) else (
    echo.
    echo [ERROR] Qt6 not found at %QT_INSTALL_DIR%
    echo Please ensure Qt6 is extracted to D:\Qt6
    echo.
    pause
    goto :manual_install
)

:done
echo.
echo Now you can build BHT Wallet Pro:
echo   d:\work0\BHT\BHTWalletPro\build_now.bat
echo.
pause
