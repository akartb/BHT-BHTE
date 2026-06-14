@echo off
echo ============================================
echo  Download Qt6 for MinGW
echo ============================================
echo.

set QT_DIR=d:\work0\BHT\Qt6

echo [INFO] Qt6 will be installed to: %QT_DIR%
echo.

if not exist "%QT_DIR%" mkdir "%QT_DIR%"

echo [1/2] Downloading Qt6 Base...
echo.

REM Use PowerShell to download
echo Downloading qtbase-everywhere-src-6.5.3.zip...
powershell -Command "Invoke-WebRequest -Uri 'https://download.qt.io/archive/qt/6.5/6.5.3/submodules/qtbase-everywhere-src-6.5.3.zip' -OutFile '%TEMP%\qtbase.zip'"

if errorlevel 1 (
    echo [ERROR] Download failed!
    echo.
    echo Please manually download from:
    echo https://download.qt.io/archive/qt/6.5/6.5.3/
    echo.
    pause
    exit /b 1
)

echo.
echo [2/2] Extracting Qt6...
powershell -Command "Expand-Archive -Path '%TEMP%\qtbase.zip' -DestinationPath '%QT_DIR%' -Force"

echo.
echo ============================================
echo  Qt6 source downloaded!
echo ============================================
echo.
echo Now you need to compile Qt6 using MinGW.
echo This may take 30-60 minutes.
echo.
pause
