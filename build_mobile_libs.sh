#!/bin/bash
# Build Mobile Libraries Script
# Creates platform-specific libquiche_engine libraries for Android and iOS
#
# Android: libquiche_engine.so (shared library) = libquiche.a + libev.a + C++ Engine
# iOS: libquiche_engine.a (static library) = libquiche.a + libev.a + C++ Engine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QUICHE_DIR="${SCRIPT_DIR}"  # Build from workspace root, not quiche/ subdirectory
OUTPUT_DIR="${SCRIPT_DIR}/mobile_libs"

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
    cargo build --lib --release --target "$target" --features cpp-engine

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

    # Create output directory
    mkdir -p "${OUTPUT_DIR}/ios/${arch}"

    # Combine all libraries into one fat static library
    echo_info "Combining libraries..."

    # Use the fat library if it exists, otherwise use the regular one
    ENGINE_LIB="${OUT_DIR}/libquiche_engine_fat.a"
    if [ ! -f "$ENGINE_LIB" ]; then
        ENGINE_LIB="${OUT_DIR}/libquiche_engine.a"
    fi

    # Method 1: Try libtool (recommended for macOS/iOS)
    if command -v libtool &> /dev/null; then
        echo_info "Using libtool to combine libraries..."
        libtool -static -o "${OUTPUT_DIR}/ios/${arch}/libquiche_engine.a" \
            "$LIBQUICHE_PATH" \
            "${OUT_DIR}/libev.a" \
            "$ENGINE_LIB"
    else
        # Method 2: Fallback to ar
        echo_warn "libtool not found, using ar as fallback..."

        # Extract all object files
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        ar -x "$LIBQUICHE_PATH"
        ar -x "${OUT_DIR}/libev.a"
        ar -x "$ENGINE_LIB"

        # Create combined archive
        ar -rcs "${OUTPUT_DIR}/ios/${arch}/libquiche_engine.a" *.o

        # Cleanup
        cd -
        rm -rf "$TEMP_DIR"
    fi

    echo_info "iOS library created: ${OUTPUT_DIR}/ios/${arch}/libquiche_engine.a"

    # Show library info
    echo_info "Library size: $(du -h "${OUTPUT_DIR}/ios/${arch}/libquiche_engine.a" | cut -f1)"
    echo_info "Library symbols:"
    nm -g "${OUTPUT_DIR}/ios/${arch}/libquiche_engine.a" | grep -E "QuicheEngine|ev_run" | head -10 || true

    # Copy header files (only once)
    if [ ! -d "${OUTPUT_DIR}/ios/include" ]; then
        cp -r quiche/engine/include "${OUTPUT_DIR}/ios/"
        echo_info "Headers copied to: ${OUTPUT_DIR}/ios/include/"
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
    cargo build --lib --release --target "$target" --features cpp-engine

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

    # Create output directory
    mkdir -p "${OUTPUT_DIR}/macos/${arch}"

    # Combine all libraries into one static library
    echo_info "Combining libraries..."

    # Use the fat library if it exists, otherwise use the regular one
    ENGINE_LIB="${OUT_DIR}/libquiche_engine_fat.a"
    if [ ! -f "$ENGINE_LIB" ]; then
        ENGINE_LIB="${OUT_DIR}/libquiche_engine.a"
    fi

    # Method 1: Try libtool (recommended for macOS)
    if command -v libtool &> /dev/null; then
        echo_info "Using libtool to combine libraries..."
        libtool -static -o "${OUTPUT_DIR}/macos/${arch}/libquiche_engine.a" \
            "$LIBQUICHE_PATH" \
            "${OUT_DIR}/libev.a" \
            "$ENGINE_LIB"
    else
        # Method 2: Fallback to ar
        echo_warn "libtool not found, using ar as fallback..."

        # Extract all object files
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        ar -x "$LIBQUICHE_PATH"
        ar -x "${OUT_DIR}/libev.a"
        ar -x "$ENGINE_LIB"

        # Create combined archive
        ar -rcs "${OUTPUT_DIR}/macos/${arch}/libquiche_engine.a" *.o

        # Cleanup
        cd -
        rm -rf "$TEMP_DIR"
    fi

    echo_info "macOS library created: ${OUTPUT_DIR}/macos/${arch}/libquiche_engine.a"

    # Show library info
    echo_info "Library size: $(du -h "${OUTPUT_DIR}/macos/${arch}/libquiche_engine.a" | cut -f1)"

    # Try to show library symbols (suppress nm version warnings)
    echo_info "Library symbols (sample):"
    if nm -g "${OUTPUT_DIR}/macos/${arch}/libquiche_engine.a" 2>/dev/null | grep -E "QuicheEngine|ev_run" | head -10; then
        :  # Success, symbols found
    else
        echo_info "Note: nm tool version mismatch (Rust LLVM newer than Xcode), but library is valid"
        echo_info "You can verify symbols with: llvm-nm or otool"
    fi

    # Copy header files (only once)
    if [ ! -d "${OUTPUT_DIR}/macos/include" ]; then
        cp -r quiche/engine/include "${OUTPUT_DIR}/macos/"
        echo_info "Headers copied to: ${OUTPUT_DIR}/macos/include/"
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

    # Convert target triple to underscore format for environment variables
    # e.g., aarch64-linux-android -> aarch64_linux_android
    local target_underscores=$(echo "$target" | tr '-' '_')

    # Set CC, CXX, and AR for this target so cc-rs can find them
    # The NDK compilers are named with API levels, e.g., aarch64-linux-android21-clang
    export CC_${target_underscores}="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang"
    export CXX_${target_underscores}="${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"
    export AR_${target_underscores}="${NDK_BIN}/${target}-ar"

    echo_info "Using CC: ${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang"
    echo_info "Using CXX: ${NDK_BIN}/${toolchain}${ANDROID_API_LEVEL}-clang++"
    echo_info "Using AR: ${NDK_BIN}/${target}-ar"

    # Build quiche for Android
    cargo build --lib --release --target "$target" --features cpp-engine

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

        # Check if all libraries exist
        if [ ! -f "$LIBQUICHE_PATH" ] || [ ! -f "$LIBEV_PATH" ] || [ ! -f "$LIBENGINE_PATH" ]; then
            echo_error "One or more required libraries not found:"
            echo_error "  libquiche.a: $([ -f "$LIBQUICHE_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libev.a: $([ -f "$LIBEV_PATH" ] && echo "✓" || echo "✗")"
            echo_error "  libquiche_engine.a: $([ -f "$LIBENGINE_PATH" ] && echo "✓" || echo "✗")"
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
        "$NDK_COMPILER" \
            -shared \
            -o "$SO_FILE" \
            -Wl,--whole-archive \
            "$LIBQUICHE_PATH" \
            "$LIBEV_PATH" \
            "$LIBENGINE_PATH" \
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
    mkdir -p "${OUTPUT_DIR}/android/${abi}"

    # Copy shared library
    cp "$SO_FILE" "${OUTPUT_DIR}/android/${abi}/"

    echo_info "Android library created: ${OUTPUT_DIR}/android/${abi}/libquiche_engine.so"

    # Show library info
    echo_info "Library size: $(du -h "${OUTPUT_DIR}/android/${abi}/libquiche_engine.so" | cut -f1)"
    echo_info "Library dependencies:"
    if command -v readelf &> /dev/null; then
        readelf -d "${OUTPUT_DIR}/android/${abi}/libquiche_engine.so" | grep NEEDED || true
    fi

    # Copy header files (only once)
    if [ ! -d "${OUTPUT_DIR}/android/include" ]; then
        cp -r quiche/engine/include "${OUTPUT_DIR}/android/"
        echo_info "Headers copied to: ${OUTPUT_DIR}/android/include/"
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
    IOS_ARCH=""       # Specific iOS architecture to build (empty = arm64 default)
    MACOS_ARCH=""     # Specific macOS architecture to build
    ANDROID_ARCH=""   # Specific Android architecture to build

    if [ $# -eq 0 ]; then
        echo_info "Usage: $0 [ios[:arch]] [macos[:arch]] [android[:arch]] [all]"
        echo_info ""
        echo_info "Options:"
        echo_info "  ios                    - Build for iOS arm64 (device)"
        echo_info "  ios:arm64              - Build for iOS arm64 (device) explicitly"
        echo_info "  ios:x86_64             - Build for iOS simulator (x86_64)"
        echo_info "  macos                  - Build for macOS (current architecture)"
        echo_info "  macos:arm64            - Build for macOS Apple Silicon (M1/M2/M3)"
        echo_info "  macos:x86_64           - Build for macOS Intel"
        echo_info "  android                - Build for Android (all architectures)"
        echo_info "  android:arm64-v8a      - Build for Android arm64-v8a only"
        echo_info "  android:armeabi-v7a    - Build for Android armeabi-v7a only"
        echo_info "  android:x86            - Build for Android x86 only"
        echo_info "  android:x86_64         - Build for Android x86_64 only"
        echo_info "  all                    - Build for all platforms"
        echo_info ""
        echo_info "Examples:"
        echo_info "  $0 ios                          # Build iOS arm64 (device)"
        echo_info "  $0 ios:x86_64                   # Build iOS simulator"
        echo_info "  $0 macos                        # Build macOS current arch"
        echo_info "  $0 macos:arm64                  # Build macOS Apple Silicon"
        echo_info "  $0 android                      # Build all Android architectures"
        echo_info "  $0 android:arm64-v8a            # Build Android arm64 only"
        echo_info "  $0 ios macos android:arm64-v8a  # Build iOS + macOS + Android"
        echo_info ""
        exit 1
    fi

    for arg in "$@"; do
        case $arg in
            ios)
                BUILD_IOS=true
                IOS_ARCH="arm64"  # Default to arm64 (device)
                ;;
            ios:*)
                BUILD_IOS=true
                IOS_ARCH="${arg#ios:}"  # Extract architecture after ':'
                # Validate architecture
                case $IOS_ARCH in
                    arm64|x86_64)
                        echo_info "Will build iOS $IOS_ARCH"
                        ;;
                    *)
                        echo_error "Invalid iOS architecture: $IOS_ARCH"
                        echo_error "Valid options: arm64 (device), x86_64 (simulator)"
                        exit 1
                        ;;
                esac
                ;;
            macos)
                BUILD_MACOS=true
                # Detect current architecture
                if [ "$(uname -m)" = "arm64" ]; then
                    MACOS_ARCH="arm64"
                else
                    MACOS_ARCH="x86_64"
                fi
                echo_info "Will build macOS $MACOS_ARCH (auto-detected)"
                ;;
            macos:*)
                BUILD_MACOS=true
                MACOS_ARCH="${arg#macos:}"  # Extract architecture after ':'
                # Validate architecture
                case $MACOS_ARCH in
                    arm64|x86_64)
                        echo_info "Will build macOS $MACOS_ARCH"
                        ;;
                    *)
                        echo_error "Invalid macOS architecture: $MACOS_ARCH"
                        echo_error "Valid options: arm64 (Apple Silicon), x86_64 (Intel)"
                        exit 1
                        ;;
                esac
                ;;
            android)
                BUILD_ANDROID=true
                ANDROID_ARCH=""  # Build all architectures
                ;;
            android:*)
                BUILD_ANDROID=true
                ANDROID_ARCH="${arg#android:}"  # Extract architecture after ':'
                # Validate architecture
                case $ANDROID_ARCH in
                    arm64-v8a|armeabi-v7a|x86|x86_64)
                        echo_info "Will build Android $ANDROID_ARCH"
                        ;;
                    *)
                        echo_error "Invalid Android architecture: $ANDROID_ARCH"
                        echo_error "Valid options: arm64-v8a, armeabi-v7a, x86, x86_64"
                        exit 1
                        ;;
                esac
                ;;
            all)
                BUILD_IOS=true
                BUILD_MACOS=true
                BUILD_ANDROID=true
                IOS_ARCH="arm64"  # Default to arm64 for 'all'
                # Auto-detect macOS architecture
                if [ "$(uname -m)" = "arm64" ]; then
                    MACOS_ARCH="arm64"
                else
                    MACOS_ARCH="x86_64"
                fi
                ANDROID_ARCH=""   # Build all architectures
                ;;
            *)
                echo_error "Unknown option: $arg"
                exit 1
                ;;
        esac
    done

    # Build for iOS
    if [ "$BUILD_IOS" = true ]; then
        # Map architecture to Rust target
        case $IOS_ARCH in
            arm64)
                IOS_TARGET="aarch64-apple-ios"
                ;;
            x86_64)
                IOS_TARGET="x86_64-apple-ios"
                ;;
            *)
                echo_error "Unknown iOS architecture: $IOS_ARCH"
                exit 1
                ;;
        esac

        if build_ios "$IOS_ARCH" "$IOS_TARGET"; then
            echo_info "✓ iOS $IOS_ARCH build successful"
        else
            echo_error "✗ iOS $IOS_ARCH build failed"
            exit 1
        fi
    fi

    # Build for macOS
    if [ "$BUILD_MACOS" = true ]; then
        # Map architecture to Rust target
        case $MACOS_ARCH in
            arm64)
                MACOS_TARGET="aarch64-apple-darwin"
                ;;
            x86_64)
                MACOS_TARGET="x86_64-apple-darwin"
                ;;
            *)
                echo_error "Unknown macOS architecture: $MACOS_ARCH"
                exit 1
                ;;
        esac

        if build_macos "$MACOS_ARCH" "$MACOS_TARGET"; then
            echo_info "✓ macOS $MACOS_ARCH build successful"
        else
            echo_error "✗ macOS $MACOS_ARCH build failed"
            exit 1
        fi
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

        # If specific architecture is requested
        if [ -n "$ANDROID_ARCH" ]; then
            echo_info "Building Android $ANDROID_ARCH only..."
            target=$(get_android_target "$ANDROID_ARCH")
            toolchain=$(get_android_toolchain "$target")

            if [ -z "$target" ]; then
                echo_error "Unknown Android architecture: $ANDROID_ARCH"
                exit 1
            fi

            if build_android "$target" "$target" "$ANDROID_ARCH" "$toolchain"; then
                echo_info "✓ Android $ANDROID_ARCH build successful"
            else
                echo_error "✗ Android $ANDROID_ARCH build failed"
                exit 1
            fi
        else
            # Build all Android architectures
            echo_info "Building all Android architectures..."

            # List of all ABIs to build
            ANDROID_ABIS="arm64-v8a armeabi-v7a x86 x86_64"

            for abi in $ANDROID_ABIS; do
                target=$(get_android_target "$abi")
                toolchain=$(get_android_toolchain "$target")

                if build_android "$target" "$target" "$abi" "$toolchain"; then
                    echo_info "✓ Android $abi build successful"
                else
                    echo_error "✗ Android $abi build failed"
                    exit 1
                fi
            done
        fi
    fi

    echo_info ""
    echo_info "============================================"
    echo_info "Build completed successfully!"
    echo_info "============================================"
    echo_info ""
    echo_info "Output directory: $OUTPUT_DIR"
    echo_info ""

    if [ "$BUILD_IOS" = true ]; then
        echo_info "iOS libraries:"
        find "$OUTPUT_DIR/ios" -name "*.a" -exec echo "  {}" \;
    fi

    if [ "$BUILD_MACOS" = true ]; then
        echo_info "macOS libraries:"
        find "$OUTPUT_DIR/macos" -name "*.a" -exec echo "  {}" \;
    fi

    if [ "$BUILD_ANDROID" = true ]; then
        echo_info "Android libraries:"
        find "$OUTPUT_DIR/android" -name "*.so" -exec echo "  {}" \;
    fi

    echo_info ""
    echo_info "Next steps:"
    echo_info "  1. Copy the libraries to your project"
    echo_info "  2. Copy the header files from include/ directory"
    echo_info "  3. Link against the libraries in your project"
}

# Run main function
main "$@"
