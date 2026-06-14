# BHC Windows 编译环境快速设置指南

## 🚨 当前环境状态

```
检测时间: 2026-03-21
操作系统: Windows
CMake: 未安装
C++ 编译器: 未安装 (cl.exe/g++/clang++ 均不可用)
Python: 3.11.9 ✓
```

---

## 🛠️ 方案一：Visual Studio 2022 (推荐)

### 步骤 1: 下载并安装 Visual Studio

1. 访问: https://visualstudio.microsoft.com/downloads/
2. 下载 **Visual Studio 2022 Community** (免费)
3. 运行安装程序

### 步骤 2: 选择工作负载

安装时选择以下工作负载：
- ✅ **使用 C++ 的桌面开发**
- ✅ **CMake 工具**
- ✅ **Windows 10/11 SDK**

### 步骤 3: 安装 CMake

```powershell
# 方法 1: 使用 winget (推荐)
winget install Kitware.CMake

# 方法 2: 使用 Chocolatey
choco install cmake

# 方法 3: 手动下载
# https://cmake.org/download/
```

### 步骤 4: 验证安装

```powershell
# 打开 Developer Command Prompt 或 PowerShell
cmake --version
cl.exe
```

---

## 🛠️ 方案二：MSYS2 + MinGW (轻量级)

### 步骤 1: 安装 MSYS2

1. 访问: https://www.msys2.org/
2. 下载并运行安装程序
3. 安装到 `C:\msys64`

### 步骤 2: 安装编译工具

```bash
# 在 MSYS2 终端中运行
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-boost mingw-w64-x86_64-openssl
```

### 步骤 3: 添加到 PATH

```powershell
# 添加到系统 PATH
$env:PATH += ";C:\msys64\mingw64\bin"
```

---

## 🛠️ 方案三：WSL2 + Ubuntu (Linux 环境)

### 步骤 1: 启用 WSL2

```powershell
# 管理员 PowerShell
wsl --install
```

### 步骤 2: 安装 Ubuntu

```powershell
wsl --install -d Ubuntu-22.04
```

### 步骤 3: 在 WSL 中安装依赖

```bash
# 进入 WSL
wsl

# 安装编译工具
sudo apt update
sudo apt install -y build-essential cmake g++-13
sudo apt install -y libboost-all-dev libssl-dev libevent-dev
```

---

## 📦 完整依赖安装脚本 (PowerShell)

```powershell
# install_deps.ps1
# 以管理员身份运行

Write-Host "=== BHC Windows 依赖安装脚本 ===" -ForegroundColor Green

# 检查 winget
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host "安装 winget..." -ForegroundColor Yellow
    # 需要从 Microsoft Store 安装 App Installer
}

# 安装 CMake
Write-Host "`n[1/5] 安装 CMake..." -ForegroundColor Cyan
winget install Kitware.CMake --accept-source-agreements --accept-package-agreements

# 安装 Visual Studio Build Tools (轻量级)
Write-Host "`n[2/5] 安装 VS Build Tools..." -ForegroundColor Cyan
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" --accept-source-agreements --accept-package-agreements

# 安装 vcpkg
Write-Host "`n[3/5] 安装 vcpkg..." -ForegroundColor Cyan
if (-not (Test-Path "C:\vcpkg")) {
    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    C:\vcpkg\bootstrap-vcpkg.bat
    C:\vcpkg\vcpkg integrate install
}

# 安装依赖库
Write-Host "`n[4/5] 安装依赖库..." -ForegroundColor Cyan
C:\vcpkg\vcpkg install boost:x64-windows openssl:x64-windows libevent:x64-windows sqlite3:x64-windows zeromq:x64-windows

# 设置环境变量
Write-Host "`n[5/5] 设置环境变量..." -ForegroundColor Cyan
$env:PATH += ";C:\Program Files\CMake\bin;C:\vcpkg\installed\x64-windows\bin"
[Environment]::SetEnvironmentVariable("PATH", $env:PATH, [EnvironmentVariableTarget]::Machine)

Write-Host "`n=== 安装完成 ===" -ForegroundColor Green
Write-Host "请重新打开终端后运行编译命令" -ForegroundColor Yellow
```

---

## 🔧 编译命令 (安装完成后)

### Visual Studio 方式

```powershell
# 打开 Developer Command Prompt for VS 2022
cd d:\work0\BHT\BHC
mkdir build
cd build

# 配置
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DUSE_PROGPOW=ON `
    -DUSE_MLDSA=ON `
    -DUSE_GUI=OFF `
    -DUSE_TESTS=ON

# 编译
cmake --build . --config Release -j 4
```

### MinGW 方式

```powershell
cd d:\work0\BHT\BHC
mkdir build
cd build

# 配置
cmake .. -G "MinGW Makefiles" `
    -DCMAKE_BUILD_TYPE=Release `
    -DUSE_PROGPOW=ON `
    -DUSE_MLDSA=ON

# 编译
mingw32-make -j4
```

---

## ⏱️ 预计安装时间

| 方案 | 下载大小 | 安装时间 | 磁盘占用 |
|------|----------|----------|----------|
| VS 2022 Community | ~8GB | 30-60分钟 | ~20GB |
| VS Build Tools | ~3GB | 15-30分钟 | ~8GB |
| MSYS2 + MinGW | ~1GB | 10-20分钟 | ~3GB |
| WSL2 + Ubuntu | ~2GB | 15-30分钟 | ~5GB |

---

## 📝 快速检查清单

安装完成后运行以下命令验证：

```powershell
# 检查 CMake
cmake --version
# 预期: cmake version 3.25+

# 检查编译器
cl.exe  # Visual Studio
# 或
g++ --version  # MinGW

# 检查依赖库
dir "C:\vcpkg\installed\x64-windows\lib"
```

---

*最后更新: 2026-03-21*
