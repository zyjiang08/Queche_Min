#!/bin/bash
# Build Mobile Libraries Script
# Creates platform-specific libquiche_engine libraries for Android and iOS
#
# Android: libquiche_engine.so (shared library) = libquiche.a + libev.a + C++ Engine
# iOS: libquiche_engine.a (static library) = libquiche.a + libev.a + C++ Engine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QUICHE_DIR="${SCRIPT_DIR}"  # Build from workspace root, not quiche/ subdirectory
LIB_DIR="${SCRIPT_DIR}/lib"
INCLUDE_DIR="${SCRIPT_DIR}/include"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check and initialize git submodules
check_submodules() {
    if [ ! -f "quiche/deps/boringssl/CMakeLists.txt" ]; then
        echo_warn "BoringSSL submodule not initialized"
        echo_info "Initializing git submodules..."

        if ! git submodule update --init --recursive; then
            echo_error "Failed to initialize git submodules"
            echo_error "Please manually run: git submodule update --init --recursive"
            return 1
        fi

        echo_info "Git submodules initialized successfully"
    else
        echo_info "Git submodules already initialized"
    fi

    return 0
}

# Check if ANDROID_NDK_HOME is set for Android builds
check_android_ndk() {
    if [ -z "$ANDROID_NDK_HOME" ]; then
        echo_error "ANDROID_NDK_HOME is not set"
        echo_info "Please set it to your Android NDK installation path"
        echo_info "Example: export ANDROID_NDK_HOME=/Users/$(whoami)/Library/Android/sdk/ndk/23.2.8568313"
        return 1
    fi

    if [ ! -d "$ANDROID_NDK_HOME" ]; then
        echo_error "ANDROID_NDK_HOME directory does not exist: $ANDROID_NDK_HOME"
        return 1
    fi

    echo_info "Using Android NDK: $ANDROID_NDK_HOME"
    return 0
}

# Build for iOS
build_ios() {
    local arch=$1
    local target=$2

    echo_info "======================================"
    echo_info "Building for iOS ($arch)..."
    echo_info "======================================"

    cd "$QUICHE_DIR"

    # Add iOS target if not already installed
    rustup target add "$target" || true

    # Build quiche for iOS
    cargo build --lib --release --target "$target" --features ffi,cpp-engine

    # Find the build output directory
    BUILD_DIR="target/${target}/release/build"
    QUICHE_BUILD=$(find "$BUILD_DIR" -name "quiche-*" -type d -exec stat -f "%m %N" {} \; | sort -rn | head -1 | cut -d' ' -f2-)
    OUT_DIR="${QUICHE_BUILD}/out"

    echo_info "Build output directory: $OUT_DIR"

    # Check if libraries exist
    if [ ! -f "${OUT_DIR}/libev.a" ]; then
        echo_error "libev.a not found in ${OUT_DIR}"
        return 1
    fi

    if [ ! -f "${OUT_DIR}/libquiche_engine.a" ] && [ ! -f "${OUT_DIR}/libquiche_engine_fat.a" ]; then
        echo_error "libquiche_engine.a not found in ${OUT_DIR}"
        return 1
    fi

    # Get the main quiche library
    LIBQUICHE_PATH="target/${target}/release/libquiche.a"
    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "libquiche.a not found at $LIBQUICHE_PATH"
        return 1
    fi

    # Get BoringSSL libraries
    LIBCRYPTO_PATH="${OUT_DIR}/build/libcrypto.a"
    LIBSSL_PATH="${OUT_DIR}/build/libssl.a"
    if [ ! -f "$LIBCRYPTO_PATH" ] || [ ! -f "$LIBSSL_PATH" ]; then
        echo_error "BoringSSL libraries not found:"
        echo_error "  libcrypto.a: $([ -f "$LIBCRYPTO_PATH" ] && echo "✓" || echo "✗")"
        echo_error "  libssl.a: $([ -f "$LIBSSL_PATH" ] && echo "✓" || echo "✗")"
        return 1
    fi

    # Create output directory
    mkdir -p "${LIB_DIR}/ios/${arch}"

    # Combine all libraries into one fat static library
    echo_info "Combining libraries..."
    echo_info "Including: libquiche.a, libev.a, libquiche_engine.a, libcrypto.a, libssl.a"

    # Use the fat library if it exists, otherwise use the regular one
    ENGINE_LIB="${OUT_DIR}/libquiche_engine_fat.a"
    if [ ! -f "$ENGINE_LIB" ]; then
        ENGINE_LIB="${OUT_DIR}/libquiche_engine.a"
    fi

    # Method 1: Try libtool (recommended for macOS/iOS)
    if command -v libtool &> /dev/null; then
        echo_info "Using libtool to combine libraries..."
        libtool -static -o "${LIB_DIR}/ios/${arch}/libquiche_engine.a" \
            "$LIBQUICHE_PATH" \
            "${OUT_DIR}/libev.a" \
            "$ENGINE_LIB" \
            "$LIBCRYPTO_PATH" \
            "$LIBSSL_PATH"
    else
        # Method 2: Fallback to ar
        echo_warn "libtool not found, using ar as fallback..."

        # Extract all object files
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        ar -x "$LIBQUICHE_PATH"
        ar -x "${OUT_DIR}/libev.a"
        ar -x "$ENGINE_LIB"
        ar -x "$LIBCRYPTO_PATH"
        ar -x "$LIBSSL_PATH"

        # Create combined archive
        ar -rcs "${LIB_DIR}/ios/${arch}/libquiche_engine.a" *.o

        # Cleanup
        cd -
        rm -rf "$TEMP_DIR"
    fi

    echo_info "iOS library created: ${LIB_DIR}/ios/${arch}/libquiche_engine.a"

    # Show library info
    echo_info "Library size: $(du -h "${LIB_DIR}/ios/${arch}/libquiche_engine.a" | cut -f1)"

    # Show library symbols (use llvm-nm from Rust toolchain, or skip if not available)
    echo_info "Library symbols (sample):"
    if command -v llvm-nm &> /dev/null; then
        llvm-nm -g "${LIB_DIR}/ios/${arch}/libquiche_engine.a" 2>/dev/null | grep -E "QuicheEngine|ev_run" | head -10 || echo_info "No symbols found (library is valid)"
    else
        echo_info "Note: llvm-nm not found, skipping symbol check (library is valid)"
        echo_info "You can verify symbols later with: llvm-nm or otool"
    fi

    # Copy header files (only once, shared by all platforms)
    if [ ! -d "${INCLUDE_DIR}" ]; then
        mkdir -p "${INCLUDE_DIR}"
        cp quiche/engine/include/* "${INCLUDE_DIR}/"
        echo_info "Headers copied to: ${INCLUDE_DIR}/"
    fi

    return 0
}

# Build for macOS
build_macos() {
    local arch=$1
    local target=$2

    echo_info "======================================"
    echo_info "Building for macOS ($arch)..."
    echo_info "======================================"

    cd "$QUICHE_DIR"

    # Add macOS target if not already installed
    rustup target add "$target" || true

    # Build quiche for macOS with cpp-engine feature
    cargo build --lib --release --target "$target" --features ffi,cpp-engine

    # Find the build output directory
    BUILD_DIR="target/${target}/release/build"
    QUICHE_BUILD=$(find "$BUILD_DIR" -name "quiche-*" -type d -exec stat -f "%m %N" {} \; | sort -rn | head -1 | cut -d' ' -f2-)
    OUT_DIR="${QUICHE_BUILD}/out"

    echo_info "Build output directory: $OUT_DIR"

    # Check if libraries exist
    if [ ! -f "${OUT_DIR}/libev.a" ]; then
        echo_error "libev.a not found in ${OUT_DIR}"
        return 1
    fi

    if [ ! -f "${OUT_DIR}/libquiche_engine.a" ] && [ ! -f "${OUT_DIR}/libquiche_engine_fat.a" ]; then
        echo_error "libquiche_engine.a not found in ${OUT_DIR}"
        return 1
    fi

    # Get the main quiche library
    LIBQUICHE_PATH="target/${target}/release/libquiche.a"
    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "libquiche.a not found at $LIBQUICHE_PATH"
        return 1
    fi

    # Get BoringSSL libraries
    LIBCRYPTO_PATH="${OUT_DIR}/build/libcrypto.a"
    LIBSSL_PATH="${OUT_DIR}/build/libssl.a"
    if [ ! -f "$LIBCRYPTO_PATH" ] || [ ! -f "$LIBSSL_PATH" ]; then
        echo_error "BoringSSL libraries not found:"
        echo_error "  libcrypto.a: $([ -f "$LIBCRYPTO_PATH" ] && echo "✓" || echo "✗")"
        echo_error "  libssl.a: $([ -f "$LIBSSL_PATH" ] && echo "✓" || echo "✗")"
        return 1
    fi

    # Create output directory
    mkdir -p "${LIB_DIR}/macos/${arch}"

    # Combine all libraries into one static library
    echo_info "Combining libraries..."
    echo_info "Including: libquiche.a, libev.a, libquiche_engine.a, libcrypto.a, libssl.a"

    # Use the fat library if it exists, otherwise use the regular one
    ENGINE_LIB="${OUT_DIR}/libquiche_engine_fat.a"
    if [ ! -f "$ENGINE_LIB" ]; then
        ENGINE_LIB="${OUT_DIR}/libquiche_engine.a"
    fi

    # Method 1: Try libtool (recommended for macOS)
    if command -v libtool &> /dev/null; then
        echo_info "Using libtool to combine libraries..."
        libtool -static -o "${LIB_DIR}/macos/${arch}/libquiche_engine.a" \
            "$LIBQUICHE_PATH" \
            "${OUT_DIR}/libev.a" \
            "$ENGINE_LIB" \
            "$LIBCRYPTO_PATH" \
            "$LIBSSL_PATH"
    else
        # Method 2: Fallback to ar
        echo_warn "libtool not found, using ar as fallback..."

        # Extract all object files
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        ar -x "$LIBQUICHE_PATH"
        ar -x "${OUT_DIR}/libev.a"
        ar -x "$ENGINE_LIB"
        ar -x "$LIBCRYPTO_PATH"
        ar -x "$LIBSSL_PATH"

        # Create combined archive
        ar -rcs "${LIB_DIR}/macos/${arch}/libquiche_engine.a" *.o

        # Cleanup
        cd -
        rm -rf "$TEMP_DIR"
    fi

    echo_info "macOS library created: ${LIB_DIR}/macos/${arch}/libquiche_engine.a"

    # Show library info
    echo_info "Library size: $(du -h "${LIB_DIR}/macos/${arch}/libquiche_engine.a" | cut -f1)"

    # Show library symbols (use llvm-nm from Rust toolchain, or skip if not available)
    echo_info "Library symbols (sample):"
    if command -v llvm-nm &> /dev/null; then
        llvm-nm -g "${LIB_DIR}/macos/${arch}/libquiche_engine.a" 2>/dev/null | grep -E "QuicheEngine|ev_run" | head -10 || echo_info "No symbols found (library is valid)"
    else
        echo_info "Note: llvm-nm not found, skipping symbol check (library is valid)"
        echo_info "You can verify symbols later with: llvm-nm or otool"
    fi

    # Copy header files (only once, shared by all platforms)
    if [ ! -d "${INCLUDE_DIR}" ]; then
        mkdir -p "${INCLUDE_DIR}"
        cp quiche/engine/include/* "${INCLUDE_DIR}/"
        echo_info "Headers copied to: ${INCLUDE_DIR}/"
    fi

    return 0
}

# Build for Android (multiple architectures)
build_android() {
    local arch=$1
    local target=$2
    local abi=$3
    local toolchain=$4

    echo_info "======================================"
    echo_info "Building for Android ($abi)..."
    echo_info "======================================"

    cd "$QUICHE_DIR"

    # Add Android target if not already installed
    rustup target add "$target" || true

    # Set environment variables for Android build
    export ANDROID_API_LEVEL=21

    # Determine NDK toolchain paths
    local HOST_TAG="darwin-x86_64"  # macOS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        HOST_TAG="linux-x86_64"
    fi

    local NDK_BIN="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/bin"

    # Generate .cargo/config.toml dynamically for this build (BEFORE cargo build!)
    echo_info "Generating .cargo/config.toml for Android NDK..."
    mkdir -p .cargo
    cat > .cargo/config.toml << CARGO_CONFIG
# Auto-generated by quiche_engine_all.sh
# NDK: ${ANDROID_NDK_HOME}

[target.aarch64-linux-android]
ar = "${NDK_BIN}/llvm-ar"
linker = "${NDK_BIN}/aarch64-linux-android${ANDROID_API_LEVEL}-clang"
rustflags = [
    "-C", "link-arg=-lgcc",
    "-C", "link-arg=-Wl,--allow-shlib-undefined",
    "-L", "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/aarch64-linux-android/${ANDROID_API_LEVEL}"
]

[target.armv7-linux-androideabi]
ar = "${NDK_BIN}/llvm-ar"
linker = "${NDK_BIN}/armv7a-linux-androideabi${ANDROID_API_LEVEL}-clang"
rustflags = [
    "-C", "link-arg=-lgcc",
    "-C", "link-arg=-Wl,--allow-shlib-undefined",
    "-L", "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/arm-linux-androideabi/${ANDROID_API_LEVEL}"
]

[target.i686-linux-android]
ar = "${NDK_BIN}/llvm-ar"
linker = "${NDK_BIN}/i686-linux-android${ANDROID_API_LEVEL}-clang"
rustflags = [
    "-C", "link-arg=-lgcc",
    "-C", "link-arg=-Wl,--allow-shlib-undefined",
    "-L", "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/i686-linux-android/${ANDROID_API_LEVEL}"
]

[target.x86_64-linux-android]
ar = "${NDK_BIN}/llvm-ar"
linker = "${NDK_BIN}/x86_64-linux-android${ANDROID_API_LEVEL}-clang"
rustflags = [
    "-C", "link-arg=-lgcc",
    "-C", "link-arg=-Wl,--allow-shlib-undefined",
    "-L", "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/x86_64-linux-android/${ANDROID_API_LEVEL}"
]
CARGO_CONFIG

    # Convert target triple to underscore format for environment variables
    # e.g., aarch64-linux-android -> aarch64_linux_android
    local target_underscores=$(echo "$target" | tr '-' '_')

    # Set CC, CXX, and AR for this target so cc-rs can find them
    # The NDK compilers are named with API levels, e.g., aarch64-linux-android21-clang
    export CC_${target_underscores}="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang"
    export CXX_${target_underscores}="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"
    export AR_${target_underscores}="${NDK_BIN}/llvm-ar"

    echo_info "Using CC: ${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang"
    echo_info "Using CXX: ${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"
    echo_info "Using AR: ${NDK_BIN}/llvm-ar"

    # Step 1: Build libquiche.a (Rust QUIC library with FFI symbols)
    echo_info "Building libquiche.a (Rust QUIC library with FFI)..."
    cargo rustc -p quiche --release --target "$target" --no-default-features --features ffi,boringssl-vendored --crate-type staticlib --lib

    # Verify libquiche.a was created
    LIBQUICHE_PATH="target/${target}/release/libquiche.a"
    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "Failed to generate libquiche.a at $LIBQUICHE_PATH"
        return 1
    fi
    echo_info "✓ libquiche.a generated successfully: $(du -h "$LIBQUICHE_PATH" | cut -f1)"

    # Step 2: Build C++ engine and link everything together
    echo_info "Building C++ engine (will link with libquiche.a)..."
    cargo build --lib --release --target "$target" --features ffi,cpp-engine

    # Find the build output directory
    BUILD_DIR="target/${target}/release/build"
    QUICHE_BUILD=$(find "$BUILD_DIR" -name "quiche-*" -type d -exec stat -f "%m %N" {} \; | sort -rn | head -1 | cut -d' ' -f2-)
    OUT_DIR="${QUICHE_BUILD}/out"

    echo_info "Build output directory: $OUT_DIR"

    # Check if shared library was created
    SO_FILE="${OUT_DIR}/libquiche_engine.so"
    if [ -f "$SO_FILE" ]; then
        echo_info "Found shared library: $SO_FILE"
    else
        echo_warn "Shared library not found, will create it manually..."

        # Get paths to static libraries
        LIBQUICHE_PATH="target/${target}/release/libquiche.a"
        LIBEV_PATH="${OUT_DIR}/libev.a"
        LIBENGINE_PATH="${OUT_DIR}/libquiche_engine.a"
        LIBCRYPTO_PATH="${OUT_DIR}/build/libcrypto.a"
        LIBSSL_PATH="${OUT_DIR}/build/libssl.a"

        # Check if all libraries exist
        if [ ! -f "$LIBQUICHE_PATH" ] || [ ! -f "$LIBEV_PATH" ] || [ ! -f "$LIBENGINE_PATH" ] || [ ! -f "$LIBCRYPTO_PATH" ] || [ ! -f "$LIBSSL_PATH" ]; then
            echo_error "One or more required libraries not found:"
            echo_error "  libquiche.a: $([ -f "$LIBQUICHE_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libev.a: $([ -f "$LIBEV_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libquiche_engine.a: $([ -f "$LIBENGINE_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libcrypto.a (BoringSSL): $([ -f "$LIBCRYPTO_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libssl.a (BoringSSL): $([ -f "$LIBSSL_PATH" ] && echo "✓" || echo "✗")"
            return 1
        fi

        # Use the NDK compiler path we set earlier
        NDK_COMPILER="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"

        if [ ! -f "$NDK_COMPILER" ]; then
            echo_error "NDK compiler not found: $NDK_COMPILER"
            return 1
        fi

        # Create shared library
        echo_info "Creating shared library with $NDK_COMPILER..."
        echo_info "Linking libraries: libquiche.a, libev.a, libquiche_engine.a, libcrypto.a, libssl.a"
        "$NDK_COMPILER" \
            -shared \
            -o "$SO_FILE" \
            -Wl,--whole-archive \
            "$LIBQUICHE_PATH" \
            "$LIBEV_PATH" \
            "$LIBENGINE_PATH" \
            "$LIBCRYPTO_PATH" \
            "$LIBSSL_PATH" \
            -Wl,--no-whole-archive \
            -lc++_shared \
            -llog \
            -lm

        if [ $? -eq 0 ]; then
            echo_info "Shared library created successfully"
        else
            echo_error "Failed to create shared library"
            return 1
        fi
    fi

    # Create output directory
    mkdir -p "${LIB_DIR}/android/${abi}"

    # Copy shared library
    cp "$SO_FILE" "${LIB_DIR}/android/${abi}/"

    echo_info "Android library created: ${LIB_DIR}/android/${abi}/libquiche_engine.so"

    # Show library info
    echo_info "Library size: $(du -h "${LIB_DIR}/android/${abi}/libquiche_engine.so" | cut -f1)"
    echo_info "Library dependencies:"
    if command -v readelf &> /dev/null; then
        readelf -d "${LIB_DIR}/android/${abi}/libquiche_engine.so" | grep NEEDED || true
    fi

    # Also create a combined static library for static linking
    echo_info "Creating combined static library..."
    LIBQUICHE_PATH="target/${target}/release/libquiche.a"
    LIBEV_PATH="${OUT_DIR}/libev.a"
    LIBENGINE_PATH="${OUT_DIR}/libquiche_engine.a"
    LIBCRYPTO_PATH="${OUT_DIR}/build/libcrypto.a"
    LIBSSL_PATH="${OUT_DIR}/build/libssl.a"

    # Extract and combine all object files into one static library
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"

    # Extract all .o files from all libraries
    ${NDK_BIN}/llvm-ar -x "$LIBQUICHE_PATH"
    ${NDK_BIN}/llvm-ar -x "$LIBEV_PATH"
    ${NDK_BIN}/llvm-ar -x "$LIBENGINE_PATH"
    ${NDK_BIN}/llvm-ar -x "$LIBCRYPTO_PATH"
    ${NDK_BIN}/llvm-ar -x "$LIBSSL_PATH"

    # Create combined archive
    ${NDK_BIN}/llvm-ar -rcs "${LIB_DIR}/android/${abi}/libquiche_engine.a" *.o

    # Cleanup
    cd - > /dev/null
    rm -rf "$TEMP_DIR"

    echo_info "Android static library created: ${LIB_DIR}/android/${abi}/libquiche_engine.a"
    echo_info "Static library size: $(du -h "${LIB_DIR}/android/${abi}/libquiche_engine.a" | cut -f1)"

    # Copy header files (only once, shared by all platforms)
    if [ ! -d "${INCLUDE_DIR}" ]; then
        mkdir -p "${INCLUDE_DIR}"
        cp quiche/engine/include/* "${INCLUDE_DIR}/"
        echo_info "Headers copied to: ${INCLUDE_DIR}/"
    fi

    return 0
}

# Main build function
main() {
    echo_info "============================================"
    echo_info "Mobile Libraries Build Script"
    echo_info "============================================"
    echo_info ""

    # Parse command line arguments
    BUILD_IOS=false
    BUILD_MACOS=false
    BUILD_ANDROID=false
    IOS_ARCHS=()       # Array of iOS architectures to build
    MACOS_ARCHS=()     # Array of macOS architectures to build
    ANDROID_ARCHS=()   # Array of Android architectures to build

    if [ $# -eq 0 ]; then
        echo_info "Usage: $0 <platform> [arch] [<platform> [arch] ...]"
        echo_info ""
        echo_info "Platforms:"
        echo_info "  ios [arch]          - Build for iOS"
        echo_info "  macos [arch]        - Build for macOS"
        echo_info "  android [arch]      - Build for Android"
        echo_info "  all                 - Build for all platforms"
        echo_info ""
        echo_info "Architectures:"
        echo_info "  iOS:     arm64 (device), x86_64 (simulator), all"
        echo_info "  macOS:   arm64 (Apple Silicon), x86_64 (Intel), all"
        echo_info "  Android: arm64-v8a, armeabi-v7a, x86, x86_64, all"
        echo_info ""
        echo_info "If [arch] is omitted:"
        echo_info "  ios      - builds arm64 (device)"
        echo_info "  macos    - builds current architecture"
        echo_info "  android  - builds all architectures"
        echo_info ""
        echo_info "Examples:"
        echo_info "  $0 ios arm64                    # iOS device"
        echo_info "  $0 ios x86_64                   # iOS simulator"
        echo_info "  $0 ios all                      # All iOS architectures"
        echo_info "  $0 macos arm64                  # macOS Apple Silicon"
        echo_info "  $0 macos all                    # All macOS architectures"
        echo_info "  $0 android arm64-v8a            # Android ARM64"
        echo_info "  $0 android all                  # All Android architectures"
        echo_info "  $0 ios arm64 android arm64-v8a  # iOS + Android"
        echo_info "  $0 all                          # All platforms"
        echo_info ""
        exit 1
    fi

    # Parse arguments
    i=1
    while [ $i -le $# ]; do
        arg="${!i}"
        case $arg in
            ios)
                BUILD_IOS=true
                # Check if next arg is an architecture
                next_i=$((i + 1))
                if [ $next_i -le $# ]; then
                    next_arg="${!next_i}"
                    case $next_arg in
                        arm64|x86_64)
                            IOS_ARCHS+=("$next_arg")
                            i=$next_i  # Skip next arg
                            ;;
                        all)
                            IOS_ARCHS=("arm64" "x86_64")
                            i=$next_i  # Skip next arg
                            ;;
                        ios|macos|android|all)
                            # Next arg is another platform, use default
                            IOS_ARCHS+=("arm64")
                            ;;
                        *)
                            echo_error "Invalid iOS architecture: $next_arg"
                            echo_error "Valid options: arm64, x86_64, all"
                            exit 1
                            ;;
                    esac
                else
                    # No next arg, use default
                    IOS_ARCHS+=("arm64")
                fi
                ;;
            macos)
                BUILD_MACOS=true
                # Check if next arg is an architecture
                next_i=$((i + 1))
                if [ $next_i -le $# ]; then
                    next_arg="${!next_i}"
                    case $next_arg in
                        arm64|x86_64)
                            MACOS_ARCHS+=("$next_arg")
                            i=$next_i  # Skip next arg
                            ;;
                        all)
                            MACOS_ARCHS=("arm64" "x86_64")
                            i=$next_i  # Skip next arg
                            ;;
                        ios|macos|android|all)
                            # Next arg is another platform, auto-detect
                            if [ "$(uname -m)" = "arm64" ]; then
                                MACOS_ARCHS+=("arm64")
                            else
                                MACOS_ARCHS+=("x86_64")
                            fi
                            ;;
                        *)
                            echo_error "Invalid macOS architecture: $next_arg"
                            echo_error "Valid options: arm64, x86_64, all"
                            exit 1
                            ;;
                    esac
                else
                    # No next arg, auto-detect
                    if [ "$(uname -m)" = "arm64" ]; then
                        MACOS_ARCHS+=("arm64")
                    else
                        MACOS_ARCHS+=("x86_64")
                    fi
                fi
                ;;
            android)
                BUILD_ANDROID=true
                # Check if next arg is an architecture
                next_i=$((i + 1))
                if [ $next_i -le $# ]; then
                    next_arg="${!next_i}"
                    case $next_arg in
                        arm64-v8a|armeabi-v7a|x86|x86_64)
                            ANDROID_ARCHS+=("$next_arg")
                            i=$next_i  # Skip next arg
                            ;;
                        all)
                            ANDROID_ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
                            i=$next_i  # Skip next arg
                            ;;
                        ios|macos|android|all)
                            # Next arg is another platform, build all
                            ANDROID_ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
                            ;;
                        *)
                            echo_error "Invalid Android architecture: $next_arg"
                            echo_error "Valid options: arm64-v8a, armeabi-v7a, x86, x86_64, all"
                            exit 1
                            ;;
                    esac
                else
                    # No next arg, build all architectures
                    ANDROID_ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
                fi
                ;;
            all)
                BUILD_IOS=true
                BUILD_MACOS=true
                BUILD_ANDROID=true
                IOS_ARCHS=("arm64")  # Default to arm64 for 'all'
                # Auto-detect macOS architecture
                if [ "$(uname -m)" = "arm64" ]; then
                    MACOS_ARCHS=("arm64")
                else
                    MACOS_ARCHS=("x86_64")
                fi
                ANDROID_ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
                ;;
            *)
                echo_error "Unknown option: $arg"
                echo_info "Run '$0' without arguments to see usage"
                exit 1
                ;;
        esac
        i=$((i + 1))
    done

    # Display build plan
    echo_info "Build plan:"
    if [ "$BUILD_IOS" = true ]; then
        echo_info "  iOS: ${IOS_ARCHS[*]}"
    fi
    if [ "$BUILD_MACOS" = true ]; then
        echo_info "  macOS: ${MACOS_ARCHS[*]}"
    fi
    if [ "$BUILD_ANDROID" = true ]; then
        echo_info "  Android: ${ANDROID_ARCHS[*]}"
    fi

    # Check and initialize git submodules before building
    echo_info ""
    if ! check_submodules; then
        exit 1
    fi
    echo_info ""

    # Build for iOS
    if [ "$BUILD_IOS" = true ]; then
        for arch in "${IOS_ARCHS[@]}"; do
            # Map architecture to Rust target
            case $arch in
                arm64)
                    target="aarch64-apple-ios"
                    ;;
                x86_64)
                    target="x86_64-apple-ios"
                    ;;
                *)
                    echo_error "Unknown iOS architecture: $arch"
                    exit 1
                    ;;
            esac

            if build_ios "$arch" "$target"; then
                echo_info "✓ iOS $arch build successful"
            else
                echo_error "✗ iOS $arch build failed"
                exit 1
            fi
        done
    fi

    # Build for macOS
    if [ "$BUILD_MACOS" = true ]; then
        for arch in "${MACOS_ARCHS[@]}"; do
            # Map architecture to Rust target
            case $arch in
                arm64)
                    target="aarch64-apple-darwin"
                    ;;
                x86_64)
                    target="x86_64-apple-darwin"
                    ;;
                *)
                    echo_error "Unknown macOS architecture: $arch"
                    exit 1
                    ;;
            esac

            if build_macos "$arch" "$target"; then
                echo_info "✓ macOS $arch build successful"
            else
                echo_error "✗ macOS $arch build failed"
                exit 1
            fi
        done
    fi

    # Build for Android
    if [ "$BUILD_ANDROID" = true ]; then
        if ! check_android_ndk; then
            exit 1
        fi

        # Helper function to get target from ABI
        get_android_target() {
            local abi=$1
            case $abi in
                arm64-v8a)
                    echo "aarch64-linux-android"
                    ;;
                armeabi-v7a)
                    echo "armv7-linux-androideabi"
                    ;;
                x86)
                    echo "i686-linux-android"
                    ;;
                x86_64)
                    echo "x86_64-linux-android"
                    ;;
                *)
                    echo ""
                    ;;
            esac
        }

        # Helper function to get toolchain from target
        get_android_toolchain() {
            local target=$1
            case $target in
                aarch64-linux-android)
                    echo "aarch64-linux-android"
                    ;;
                armv7-linux-androideabi)
                    echo "armv7a-linux-androideabi"
                    ;;
                i686-linux-android)
                    echo "i686-linux-android"
                    ;;
                x86_64-linux-android)
                    echo "x86_64-linux-android"
                    ;;
                *)
                    echo ""
                    ;;
            esac
        }

        # Build all requested Android architectures
        for abi in "${ANDROID_ARCHS[@]}"; do
            target=$(get_android_target "$abi")
            toolchain=$(get_android_toolchain "$target")

            if [ -z "$target" ]; then
                echo_error "Unknown Android architecture: $abi"
                exit 1
            fi

            if build_android "$target" "$target" "$abi" "$toolchain"; then
                echo_info "✓ Android $abi build successful"
            else
                echo_error "✗ Android $abi build failed"
                exit 1
            fi
        done
    fi

    echo_info ""
    echo_info "============================================"
    echo_info "Build completed successfully!"
    echo_info "============================================"
    echo_info ""
    echo_info "Output structure:"
    echo_info "  lib/       - Platform libraries"
    echo_info "  include/   - Header files"
    echo_info ""

    if [ "$BUILD_IOS" = true ]; then
        echo_info "iOS libraries:"
        find "$LIB_DIR/ios" -name "*.a" -exec echo "  {}" \; 2>/dev/null || true
    fi

    if [ "$BUILD_MACOS" = true ]; then
        echo_info "macOS libraries:"
        find "$LIB_DIR/macos" -name "*.a" -exec echo "  {}" \; 2>/dev/null || true
    fi

    if [ "$BUILD_ANDROID" = true ]; then
        echo_info "Android libraries:"
        find "$LIB_DIR/android" -name "*.so" -exec echo "  {}" \; 2>/dev/null || true
    fi

    echo_info ""
    echo_info "Header files:"
    if [ -d "$INCLUDE_DIR" ]; then
        find "$INCLUDE_DIR" -name "*.h" -exec echo "  {}" \;
    fi

    echo_info ""
    echo_info "Next steps:"
    echo_info "  1. Copy lib/ and include/ to your project"
    echo_info "  2. Link against the libraries in your project"
    echo_info "  3. Include the header files: #include \"quiche_engine.h\""
}

# Run main function
main "$@"
