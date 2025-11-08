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
DEFAULT_TARGET_DIR="${QUICHE_DIR}/target"
TARGET_DIR="${MOBILE_CARGO_TARGET_DIR:-${DEFAULT_TARGET_DIR}}"
mkdir -p "$TARGET_DIR"
export CARGO_TARGET_DIR="$TARGET_DIR"

DEFAULT_MOBILE_FEATURES="boringssl-vendored,ffi,cpp-engine,qlog"
MOBILE_FEATURES="${MOBILE_FEATURES:-$DEFAULT_MOBILE_FEATURES}"
MOBILE_ANDROID_FEATURES="${MOBILE_ANDROID_FEATURES:-$MOBILE_FEATURES}"
MOBILE_IOS_FEATURES="${MOBILE_IOS_FEATURES:-$MOBILE_FEATURES}"
MOBILE_MACOS_FEATURES="${MOBILE_MACOS_FEATURES:-$MOBILE_FEATURES}"

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

# Detect strip tool (prefer NDK llvm-strip, fall back to host tools).
detect_strip_tool() {
    local strip_tool=""

    if [ -n "${NDK_BIN:-}" ] && [ -x "${NDK_BIN}/llvm-strip" ]; then
        strip_tool="${NDK_BIN}/llvm-strip"
    elif command -v llvm-strip > /dev/null 2>&1; then
        strip_tool=$(command -v llvm-strip)
    elif command -v strip > /dev/null 2>&1; then
        strip_tool=$(command -v strip)
    fi

    echo "$strip_tool"
}

# Run size tool (llvm-size or size) on a binary and append to report file.
run_size_tool() {
    local binary_path="$1"
    local report_file="$2"
    local size_tool=""

    if command -v llvm-size > /dev/null 2>&1; then
        size_tool=$(command -v llvm-size)
    elif command -v size > /dev/null 2>&1; then
        size_tool=$(command -v size)
    fi

    if [ -n "$size_tool" ]; then
        {
            echo "### $(basename "$size_tool") output for $(basename "$binary_path")"
            "$size_tool" "$binary_path"
            echo ""
        } >> "$report_file"
    else
        {
            echo "### Size tool"
            echo "- Not found on PATH"
            echo ""
        } >> "$report_file"
    fi
}

strip_shared_library() {
    local binary_path="$1"
    local strip_tool
    strip_tool=$(detect_strip_tool)

    if [ -z "$strip_tool" ]; then
        echo_warn "Strip tool not found; keeping unstripped binary"
        return
    fi

    local dbg_path="${binary_path}.dbg"
    cp "$binary_path" "$dbg_path"
    if "$strip_tool" --strip-unneeded "$binary_path"; then
        echo_info "Stripped $(basename "$binary_path") (debug copy: $(basename "$dbg_path"))"
    else
        echo_warn "Failed to strip $(basename "$binary_path")"
        rm -f "$dbg_path"
    fi
}

write_size_report() {
    local platform="$1"
    local arch="$2"
    local so_path="$3"
    local static_path="$4"

    local report_dir="${SCRIPT_DIR}/docs/size-analysis"
    mkdir -p "$report_dir"
    local report_file="${report_dir}/latest-${platform}-${arch}.md"

    {
        echo "# ${platform^} ${arch} Build Report"
        echo "- Generated: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
        echo "- Commit: $(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
        echo ""
        if [ -f "$so_path" ]; then
            echo "## Shared Library"
            echo "- Path: $so_path"
            echo "- Size: $(du -h "$so_path" | cut -f1)"
            echo ""
        else
            echo "## Shared Library"
            echo "- Missing ($so_path)"
            echo ""
        fi
        if [ -f "$static_path" ]; then
            echo "## Static Library"
            echo "- Path: $static_path"
            echo "- Size: $(du -h "$static_path" | cut -f1)"
            echo ""
        else
            echo "## Static Library"
            echo "- Missing ($static_path)"
            echo ""
        fi
    } > "$report_file"

    if [ -f "$so_path" ]; then
        run_size_tool "$so_path" "$report_file"
    fi

    echo_info "Wrote size report: $report_file"
}

get_platform_features() {
    local platform="$1"
    case "$platform" in
        android)
            echo "$MOBILE_ANDROID_FEATURES"
            ;;
        ios)
            echo "$MOBILE_IOS_FEATURES"
            ;;
        macos)
            echo "$MOBILE_MACOS_FEATURES"
            ;;
        *)
            echo "$MOBILE_FEATURES"
            ;;
    esac
}

# Enable minimal BoringSSL mode by default to strip error strings / stdio.
if [ -z "${QUICHE_MINIMAL_BSSL:-}" ]; then
    export QUICHE_MINIMAL_BSSL=1
    echo_info "QUICHE_MINIMAL_BSSL enabled (strips BoringSSL error strings / stdio)"
else
    echo_info "QUICHE_MINIMAL_BSSL already set to ${QUICHE_MINIMAL_BSSL}"
fi

# Ensure temporary files live on the same filesystem as the workspace to avoid
# cross-device rename errors when Cargo persists artifacts.
TMP_DIR_LOCAL="${TARGET_DIR}/tmp"
mkdir -p "$TMP_DIR_LOCAL"
export TMPDIR="$TMP_DIR_LOCAL"
export CARGO_TARGET_TMPDIR="$TMP_DIR_LOCAL"
export RUSTC_TMPDIR="$TMP_DIR_LOCAL"
echo_info "TMPDIR set to $TMP_DIR_LOCAL"

# Determine the latest quiche build directory inside a target's build path.
# Uses python when available (works on macOS/Linux) and falls back to shell utilities.
get_latest_quiche_build_dir() {
    local build_root="$1"

    if [ ! -d "$build_root" ]; then
        return 0
    fi

    local latest_dir=""

    if command -v python3 >/dev/null 2>&1; then
        latest_dir=$(python3 - "$build_root" <<'PY'
import os, sys
root = sys.argv[1]
latest = ""
latest_mtime = -1.0
if os.path.isdir(root):
    for name in os.listdir(root):
        if not name.startswith("quiche-"):
            continue
        path = os.path.join(root, name)
        if not os.path.isdir(path):
            continue
        mtime = os.path.getmtime(path)
        if mtime > latest_mtime:
            latest_mtime = mtime
            latest = path
if latest:
    print(latest)
PY
)
    else
        shopt -s nullglob
        local candidates=("$build_root"/quiche-*)
        shopt -u nullglob
        if [ ${#candidates[@]} -gt 0 ]; then
            latest_dir=$(ls -td "${candidates[@]}" 2>/dev/null | head -1)
        fi
    fi

    if [ -n "$latest_dir" ]; then
        echo "$latest_dir"
    fi
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

    local ios_features
    ios_features=$(get_platform_features "ios")
    local ios_feature_args=()
    if [ -n "$ios_features" ]; then
        ios_feature_args+=(--features "$ios_features")
    fi

    # Build quiche for iOS with customizable feature set
    cargo build --lib --release --target "$target" \
        --no-default-features \
        "${ios_feature_args[@]}"

    # Find the build output directory
    BUILD_DIR="${TARGET_DIR}/${target}/release/build"
    QUICHE_BUILD=$(get_latest_quiche_build_dir "$BUILD_DIR")
    if [ -z "$QUICHE_BUILD" ]; then
        echo_error "Could not locate build output in $BUILD_DIR"
        return 1
    fi
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
    LIBQUICHE_PATH="${TARGET_DIR}/${target}/release/libquiche.a"
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

    local macos_features
    macos_features=$(get_platform_features "macos")
    local macos_feature_args=()
    if [ -n "$macos_features" ]; then
        macos_feature_args+=(--features "$macos_features")
    fi

    # Build quiche for macOS with customizable features
    cargo build --lib --release --target "$target" \
        --no-default-features \
        "${macos_feature_args[@]}"

    # Find the build output directory
    BUILD_DIR="${TARGET_DIR}/${target}/release/build"
    QUICHE_BUILD=$(get_latest_quiche_build_dir "$BUILD_DIR")
    if [ -z "$QUICHE_BUILD" ]; then
        echo_error "Could not locate build output in $BUILD_DIR"
        return 1
    fi
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
    LIBQUICHE_PATH="${TARGET_DIR}/${target}/release/libquiche.a"
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

    local android_features
    android_features=$(get_platform_features "android")
    local android_feature_args=()
    if [ -n "$android_features" ]; then
        android_feature_args+=(--features "$android_features")
    fi

    # Step 1: Build libquiche.a (Rust QUIC library with FFI symbols)
    echo_info "Building libquiche.a (Rust QUIC library with FFI)..."
    cargo rustc -p quiche --release --target "$target" --no-default-features \
        "${android_feature_args[@]}" --crate-type staticlib --lib

    # Verify libquiche.a was created
    LIBQUICHE_PATH="${TARGET_DIR}/${target}/release/libquiche.a"
    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "Failed to generate libquiche.a at $LIBQUICHE_PATH"
        return 1
    fi
    echo_info "✓ libquiche.a generated successfully: $(du -h "$LIBQUICHE_PATH" | cut -f1)"

    # Step 2: Build C++ engine and link everything together (feature set customizable)
    echo_info "Building C++ engine (will link with libquiche.a)..."
    cargo build --lib --release --target "$target" \
        --no-default-features \
        "${android_feature_args[@]}"

    # Find the build output directory
    BUILD_DIR="${TARGET_DIR}/${target}/release/build"
    QUICHE_BUILD=$(get_latest_quiche_build_dir "$BUILD_DIR")
    if [ -z "$QUICHE_BUILD" ]; then
        echo_error "Could not locate build output in $BUILD_DIR"
        return 1
    fi
    OUT_DIR="${QUICHE_BUILD}/out"

    echo_info "Build output directory: $OUT_DIR"

    # Always relink shared library to apply export controls
    SO_FILE="${OUT_DIR}/libquiche_engine.so"
    LIBQUICHE_PATH="${TARGET_DIR}/${target}/release/libquiche.a"

    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "Required library not found: libquiche.a ($LIBQUICHE_PATH)"
        return 1
    fi

    local export_map="${SCRIPT_DIR}/tools/mobile/exported_symbols.map"
    local version_script_args=()
    if [ -f "$export_map" ]; then
        version_script_args+=(-Wl,--version-script="$export_map")
    else
        echo_warn "Export map not found at $export_map; exporting all symbols"
    fi

    local ndk_compiler="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"
    if [ ! -f "$ndk_compiler" ]; then
        echo_error "NDK compiler not found: $ndk_compiler"
        return 1
    fi

    echo_info "Linking shared library with $ndk_compiler (using version script)..."
    if "$ndk_compiler" \
        -shared \
        -o "$SO_FILE" \
        -Wl,--whole-archive \
        "$LIBQUICHE_PATH" \
        -Wl,--no-whole-archive \
        "${version_script_args[@]}" \
        -lc++_shared \
        -llog \
        -lm; then
        echo_info "Shared library created successfully"
    else
        echo_error "Failed to create shared library"
        return 1
    fi

    # Create output directory
    mkdir -p "${LIB_DIR}/android/${abi}"

    # Copy shared library
    local so_dest="${LIB_DIR}/android/${abi}/libquiche_engine.so"
    cp "$SO_FILE" "$so_dest"

    # Strip shared library (keep .dbg copy)
    strip_shared_library "$so_dest"

    echo_info "Android library created: $so_dest"

    # Show library info
    echo_info "Library size: $(du -h "${LIB_DIR}/android/${abi}/libquiche_engine.so" | cut -f1)"
    echo_info "Library dependencies:"
    if command -v readelf &> /dev/null; then
        readelf -d "${LIB_DIR}/android/${abi}/libquiche_engine.so" | grep NEEDED || true
    fi

    # Also create a combined static library for static linking
    echo_info "Creating combined static library..."
    LIBQUICHE_PATH="${TARGET_DIR}/${target}/release/libquiche.a"

    if [ ! -f "$LIBQUICHE_PATH" ]; then
        echo_error "libquiche.a not found when creating static library"
        return 1
    fi

    local static_dest="${LIB_DIR}/android/${abi}/libquiche_engine.a"
    cp "$LIBQUICHE_PATH" "$static_dest"

    echo_info "Android static library created: $static_dest"
    echo_info "Static library size: $(du -h "$static_dest" | cut -f1)"

    # Write size report for this architecture
    write_size_report "android" "$abi" "$so_dest" "$static_dest"

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
