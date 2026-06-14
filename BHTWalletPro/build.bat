@echo off
REM BHT Wallet Pro - Unified Build Script (VS 2026 + Qt 6.5.0 + NMake)
cd /d "%~dp0"

set MSVC_PATH=D:\11\VC\Tools\MSVC\14.50.35717
set KITS_PATH=C:\Program Files (x86)\Windows Kits\10
set KITS_VER=10.0.26100.0
set QT_PATH=D:\Qt\6.5.0\msvc2019_64
set OPENSSL_PATH=D:\PostgreSQL-18

set PATH=%MSVC_PATH%\bin\Hostx64\x64;%QT_PATH%\bin;%KITS_PATH%\bin\%KITS_VER%\x64;%PATH%
set INCLUDE=%MSVC_PATH%\include;%KITS_PATH%\Include\%KITS_VER%\ucrt;%KITS_PATH%\Include\%KITS_VER%\shared;%KITS_PATH%\Include\%KITS_VER%\um
set LIB=%MSVC_PATH%\lib\x64;%KITS_PATH%\Lib\%KITS_VER%\ucrt\x64;%KITS_PATH%\Lib\%KITS_VER%\um\x64

if not exist build mkdir build
cd build

cmake -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH=%QT_PATH% ^
    -DOPENSSL_ROOT_DIR=%OPENSSL_PATH% ^
    ..

cmake --build . --config Release

echo.
echo ==========================================
echo Build Complete: bin\bht-wallet.exe
echo ==========================================
dir bin\bht-wallet.exe 2>nul

if exist bin\bht-wallet.exe (
    echo.
    echo Deploying Qt runtime...
    "%QT_PATH%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime --dir "%CD%\bin" "%CD%\bin\bht-wallet.exe"

    echo.
    echo Deploying OpenSSL runtime...
    if exist "%OPENSSL_PATH%\bin\libssl-3-x64.dll" copy /Y "%OPENSSL_PATH%\bin\libssl-3-x64.dll" "%CD%\bin\" >nul
    if exist "%OPENSSL_PATH%\bin\libcrypto-3-x64.dll" copy /Y "%OPENSSL_PATH%\bin\libcrypto-3-x64.dll" "%CD%\bin\" >nul

    echo.
    echo Run with: bin\bht-wallet.exe
) else (
    echo Build FAILED - check errors above.
)
