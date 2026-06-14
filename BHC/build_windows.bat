@echo off
REM BHC Windows 编译脚本
REM 需要先安装 Visual Studio 2022 或 Build Tools

echo ============================================================
echo BHC Windows 编译脚本
echo ============================================================
echo.

REM 检查 CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] CMake 未安装
    echo 请运行: winget install Kitware.CMake
    pause
    exit /b 1
)

REM 检查 Visual Studio
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [提示] 未检测到 Visual Studio 编译器
    echo 尝试查找 VS 安装路径...
    
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo [错误] 未找到 Visual Studio 2022
        echo 请安装 Visual Studio 2022 Build Tools:
        echo winget install Microsoft.VisualStudio.2022.BuildTools
        pause
        exit /b 1
    )
)

echo [OK] 编译环境检测通过
echo.

REM 创建构建目录
if not exist "build" mkdir build
cd build

REM 配置
echo [1/2] 配置项目...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DUSE_PROGPOW=ON ^
    -DUSE_MLDSA=ON ^
    -DUSE_GUI=OFF ^
    -DUSE_TESTS=ON ^
    -DUSE_WALLET=ON

if %errorlevel% neq 0 (
    echo [错误] CMake 配置失败
    pause
    exit /b 1
)

REM 编译
echo.
echo [2/2] 编译项目 (Release 模式)...
cmake --build . --config Release -j 4

if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo ============================================================
echo 编译完成!
echo ============================================================
echo.
echo 二进制文件位置: %cd%\src\Release\
echo.
echo 下一步:
echo   1. 启动节点: src\Release\bitcoind.exe -regtest -daemon
echo   2. 验证创世块: src\Release\bitcoin-cli.exe -regtest getblockhash 0
echo.

pause
