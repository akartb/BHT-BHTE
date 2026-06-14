#!/usr/bin/env bash
# BHC Build Script
# Copyright (c) 2026 BHC Developers

set -e

echo "============================================================"
echo "BHC Node Build Script"
echo "============================================================"
echo ""

BUILD_DIR="build"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

check_dependencies() {
    echo "--- Checking Dependencies ---"
    
    local missing=()
    
    command -v cmake >/dev/null 2>&1 || missing+=("cmake")
    command -v make >/dev/null 2>&1 || missing+=("make")
    command -v g++ >/dev/null 2>&1 || missing+=("g++")
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "❌ Missing dependencies: ${missing[*]}"
        echo "   Please install them first."
        exit 1
    fi
    
    echo "✅ All dependencies found"
    echo ""
}

configure_build() {
    echo "--- Configuring Build ---"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DUSE_PROGPOW=ON \
        -DUSE_MLDSA=ON \
        -DUSE_GUI=OFF \
        -DUSE_TESTS=ON \
        -DUSE_WALLET=ON
    
    echo "✅ Configuration complete"
    echo ""
}

build_project() {
    echo "--- Building BHC ---"
    echo "   Using $JOBS parallel jobs"
    
    make -j"$JOBS"
    
    echo "✅ Build complete"
    echo ""
}

run_tests() {
    echo "--- Running Tests ---"
    
    if [ -f "./test_bhc" ]; then
        ./test_bhc --log_level=test_suite
        echo "✅ Tests passed"
    else
        echo "⚠️  Test binary not found"
    fi
    echo ""
}

install_binaries() {
    echo "--- Installing Binaries ---"
    
    local install_dir="${HOME}/.local/bin"
    mkdir -p "$install_dir"
    
    if [ -f "./bitcoind" ]; then
        cp ./bitcoind "$install_dir/bhcd"
        echo "✅ Installed: $install_dir/bhcd"
    fi
    
    if [ -f "./bitcoin-cli" ]; then
        cp ./bitcoin-cli "$install_dir/bhc-cli"
        echo "✅ Installed: $install_dir/bhc-cli"
    fi
    
    if [ -f "./bitcoin-tx" ]; then
        cp ./bitcoin-tx "$install_dir/bhc-tx"
        echo "✅ Installed: $install_dir/bhc-tx"
    fi
    
    echo ""
    echo "Add to PATH: export PATH=\"\$PATH:$install_dir\""
    echo ""
}

print_summary() {
    echo "============================================================"
    echo "Build Summary"
    echo "============================================================"
    echo ""
    echo "Binaries:"
    ls -la bitcoind bitcoin-cli bitcoin-tx 2>/dev/null || echo "  Not found"
    echo ""
    echo "Next steps:"
    echo "  1. Start regtest node:"
    echo "     ./bitcoind -regtest -daemon"
    echo ""
    echo "  2. Verify genesis block:"
    echo "     ./bitcoin-cli -regtest getblockhash 0"
    echo ""
    echo "  3. Run hybrid signature test:"
    echo "     python3 ../tools/bhc_hybrid_test.py"
    echo ""
}

main() {
    check_dependencies
    configure_build
    build_project
    run_tests
    install_binaries
    print_summary
}

main "$@"
