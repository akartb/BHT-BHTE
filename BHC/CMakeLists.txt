# BHC CMakeLists.txt - 严谨版
# Copyright (c) 2026 BHC Developers
# 与实际源文件完全匹配的构建配置

cmake_minimum_required(VERSION 3.22)
project(BHC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_PROGPOW "Enable ProgPoW mining algorithm" ON)
option(USE_MLDSA "Enable ML-DSA post-quantum signatures" ON)
option(USE_TESTS "Build test suite" ON)
option(DOWNLOAD_LIBOQS "Download liboqs if not found" ON)

if(MSVC)
    add_compile_options(/W4 /utf-8 /EHsc)
    add_definitions(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS -D_MBCS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

set(BHC_CRYPTO_SOURCES
    src/crypto/progpow.cpp
    src/crypto/mldsa.cpp
)

set(BHC_CORE_SOURCES
    src/pow.cpp
    src/chainparams.cpp
    src/script/hybrid_sig.cpp
)

set(BHC_CORE_HEADERS
    src/crypto/progpow.h
    src/crypto/mldsa.h
    src/pow.h
    src/chainparams.h
    src/script/hybrid_sig.h
)

if(USE_PROGPOW)
    add_definitions(-DUSE_PROGPOW=1)
    message(STATUS "ProgPoW enabled")
endif()

if(USE_MLDSA)
    add_definitions(-DUSE_MLDSA=1)

    set(LIBOQS_DIR "${CMAKE_SOURCE_DIR}/liboqs" CACHE PATH "liboqs directory")

    if(NOT EXISTS "${LIBOQS_DIR}/CMakeLists.txt")
        if(DOWNLOAD_LIBOQS)
            message(STATUS "Downloading liboqs...")
            include(ExternalProject)
            ExternalProject_Add(liboqs_download
                GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
                GIT_TAG main
                SOURCE_DIR "${LIBOQS_DIR}"
                BINARY_DIR "${LIBOQS_DIR}/build"
                INSTALL_COMMAND ""
                CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DBUILD_SHARED_LIBS=OFF -DOQS_BUILD_ONLY_LIB=ON
            )
            set(LIBOQS_LIBRARY liboqs_download)
        else()
            message(FATAL_ERROR "liboqs not found at ${LIBOQS_DIR}. Set DOWNLOAD_LIBOQS=ON or provide liboqs.")
        endif()
    else()
        add_subdirectory(${LIBOQS_DIR} liboqs_build EXCLUDE_FROM_ALL)
        set(LIBOQS_LIBRARY liboqs)
    endif()

    message(STATUS "ML-DSA enabled with liboqs")
else()
    message(STATUS "ML-DSA disabled")
endif()

add_library(bhc_crypto STATIC ${BHC_CRYPTO_SOURCES})
target_include_directories(bhc_crypto PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/crypto
)
target_compile_definitions(bhc_crypto PRIVATE USE_PROGPOW=${USE_PROGPOW} USE_MLDSA=${USE_MLDSA})

if(LIBOQS_LIBRARY)
    if(LIBOQS_LIBRARY STREQUAL "liboqs_download")
        add_dependencies(bhc_crypto liboqs_download)
    endif()
    target_link_libraries(bhc_crypto ${LIBOQS_LIBRARY})
    target_include_directories(bhc_crypto PRIVATE
        ${LIBOQS_DIR}/src
        ${LIBOQS_DIR}/include
    )
endif()

add_library(bhc_core STATIC ${BHC_CORE_SOURCES})
target_include_directories(bhc_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/crypto
    ${CMAKE_SOURCE_DIR}/src/script
)
target_link_libraries(bhc_core bhc_crypto)

add_executable(bhc_standalone
    tools/standalone_test.cpp
)
target_link_libraries(bhc_standalone bhc_crypto bhc_core)

if(USE_TESTS)
    enable_testing()

    add_executable(test_crypto
        src/test/crypto_tests.cpp
    )
    target_link_libraries(test_crypto bhc_crypto bhc_core)

    add_executable(test_mldsa
        src/test/mldsa_tests.cpp
    )
    target_link_libraries(test_mldsa bhc_crypto bhc_core)

    add_test(NAME CryptoTests COMMAND test_crypto)
    add_test(NAME MLDSTests COMMAND test_mldsa)
endif()

message(STATUS "")
message(STATUS "BHC Build Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  ProgPoW:        ${USE_PROGPOW}")
message(STATUS "  ML-DSA:         ${USE_MLDSA}")
message(STATUS "  Tests:          ${USE_TESTS}")
message(STATUS "")
message(STATUS "Source Files:")
message(STATUS "  Crypto:         ${BHC_CRYPTO_SOURCES}")
message(STATUS "  Core:           ${BHC_CORE_SOURCES}")
message(STATUS "")
