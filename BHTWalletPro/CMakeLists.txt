cmake_minimum_required(VERSION 3.16)

project(BHTWalletPro
    VERSION 1.0.0
    DESCRIPTION "BHT Wallet Pro - BHC/BHTE Dual-Coin HD Wallet"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -------------------------------------------------------------------
# Qt6
# -------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# -------------------------------------------------------------------
# OpenSSL (from PostgreSQL bundled)
# -------------------------------------------------------------------
set(OPENSSL_ROOT_DIR "D:/PostgreSQL-18")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
set(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib")

# -------------------------------------------------------------------
# Qt GUI layer sources
# -------------------------------------------------------------------
set(QT_SOURCES
    src/qt/main.cpp
    src/qt/bitcoingui.cpp
    src/qt/clientmodel.cpp
    src/qt/optionsmodel.cpp
    src/qt/walletmodel.cpp
    src/qt/transactionview.cpp
    src/qt/transactionrecord.cpp
    src/qt/addressbookpage.cpp
    src/qt/overviewpage.cpp
    src/qt/sendcoinsdialog.cpp
    src/qt/receivecoinsdialog.cpp
    src/config/networkconfig.cpp
)

set(QT_HEADERS
    src/qt/bitcoingui.h
    src/qt/clientmodel.h
    src/qt/optionsmodel.h
    src/qt/walletmodel.h
    src/qt/transactionview.h
    src/qt/transactionrecord.h
    src/qt/addressbookpage.h
    src/qt/overviewpage.h
    src/qt/sendcoinsdialog.h
    src/qt/receivecoinsdialog.h
    src/config/networkconfig.h
)

set(QT_RESOURCES
    src/qt/resources/bht.qrc
)

# -------------------------------------------------------------------
# Backend layer sources (core business logic)
# -------------------------------------------------------------------
set(BACKEND_SOURCES
    src/wallet/wallet.cpp
    src/crypto/mldsa_signer.cpp
    src/layer2/layer2_client.cpp
    src/hardware/hardware_wallet.cpp
)

# -------------------------------------------------------------------
# Executable
# -------------------------------------------------------------------
add_executable(bht-wallet WIN32
    ${QT_SOURCES}
    ${QT_HEADERS}
    ${QT_RESOURCES}
    ${BACKEND_SOURCES}
)

# -------------------------------------------------------------------
# Include directories
# -------------------------------------------------------------------
target_include_directories(bht-wallet PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
)

# -------------------------------------------------------------------
# Link libraries
# -------------------------------------------------------------------
target_link_directories(bht-wallet PRIVATE ${OPENSSL_LIB_DIR})

target_link_libraries(bht-wallet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    "${OPENSSL_LIB_DIR}/libssl.lib"
    "${OPENSSL_LIB_DIR}/libcrypto.lib"
    ws2_32
    crypt32
)

# -------------------------------------------------------------------
# Compiler definitions
# -------------------------------------------------------------------
target_compile_definitions(bht-wallet PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

# -------------------------------------------------------------------
# Output directory
# -------------------------------------------------------------------
set_target_properties(bht-wallet PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
