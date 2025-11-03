#!/bin/bash
# Optimized build script for Android
# Builds quiche with maximum size optimization

set -e

# Configuration
API_LEVEL=21  # Minimum API level (Android 5.0)
ARCHS=(arm64-v8a armeabi-v7a x86_64)  # Common architectures

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check prerequisites
echo -e "${BLUE}=== Android Optimized Build for quiche ===${NC}"
echo ""

if [ -z "${ANDROID_NDK_HOME}" ]; then
    echo -e "${RED}Error: ANDROID_NDK_HOME not set${NC}"
    echo "Please set ANDROID_NDK_HOME to your Android NDK installation"
    echo "Example: export ANDROID_NDK_HOME=/usr/local/share/android-ndk"
    exit 1
fi

if ! command -v cargo &> /dev/null; then
    echo -e "${RED}Error: cargo not found${NC}"
    exit 1
fi

if ! command -v cargo-ndk &> /dev/null; then
    echo -e "${YELLOW}Warning: cargo-ndk not found${NC}"
    echo "Installing cargo-ndk..."
    cargo install cargo-ndk
fi

echo -e "${GREEN}✓ Prerequisites checked${NC}"
echo ""

# Build configuration
echo -e "${BLUE}Build Configuration:${NC}"
echo "  API Level: $API_LEVEL"
echo "  Architectures: ${ARCHS[*]}"
echo "  Optimizations: opt-level=z, lto=fat, strip=true"
echo "  Features: ffi, boringssl-vendored (no default features)"
echo ""

# Summary of optimizations applied
echo -e "${BLUE}Optimizations Applied:${NC}"
echo "  • Compiler: opt-level=z, LTO=fat, codegen-units=1"
echo "  • Linker: --gc-sections, --strip-all, --icf=all"
echo "  • Symbols: --exclude-libs,ALL (hide BoringSSL)"
echo "  • Target: cortex-a53 with NEON+crypto"
echo "  • Expected size: 65-70% reduction (28MB → 8-10MB)"
echo ""

read -p "Continue with build? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Build cancelled."
    exit 0
fi

# Start build
echo ""
echo -e "${BLUE}=== Starting Build ===${NC}"
echo ""

# Store build times and sizes
declare -A build_times
declare -A build_sizes

for arch in "${ARCHS[@]}"; do
    echo -e "${YELLOW}Building for $arch...${NC}"

    # Map architecture to Rust target
    case $arch in
        arm64-v8a)
            rust_target="aarch64-linux-android"
            ;;
        armeabi-v7a)
            rust_target="armv7-linux-androideabi"
            ;;
        x86_64)
            rust_target="x86_64-linux-android"
            ;;
        x86)
            rust_target="i686-linux-android"
            ;;
        *)
            echo -e "${RED}Unknown architecture: $arch${NC}"
            continue
            ;;
    esac

    # Start timing
    start_time=$(date +%s)

    # Build with cargo-ndk
    # The .cargo/config.toml already has all the optimization flags
    cargo ndk \
        -t $arch \
        -p $API_LEVEL \
        -- build \
        --release \
        --no-default-features \
        --features ffi,boringssl-vendored

    # End timing
    end_time=$(date +%s)
    build_time=$((end_time - start_time))
    build_times[$arch]=$build_time

    # Check output
    lib_so="target/$rust_target/release/libquiche.so"
    lib_a="target/$rust_target/release/libquiche.a"

    if [ -f "$lib_so" ]; then
        lib_path="$lib_so"
    elif [ -f "$lib_a" ]; then
        lib_path="$lib_a"
    else
        echo -e "${RED}✗ Build failed: no library found${NC}"
        continue
    fi

    # Additional stripping with NDK tools
    echo "  Post-processing with llvm-strip..."
    ndk_strip="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/*/bin/llvm-strip"

    # Find the actual strip tool
    strip_tool=$(find $ANDROID_NDK_HOME/toolchains/llvm/prebuilt -name "llvm-strip" 2>/dev/null | head -1)

    if [ -n "$strip_tool" ] && [ -f "$strip_tool" ]; then
        $strip_tool "$lib_path" 2>/dev/null || echo "  (strip failed, continuing...)"
    fi

    # Get size
    if [[ "$OSTYPE" == "darwin"* ]]; then
        size=$(stat -f%z "$lib_path")
    else
        size=$(stat -c%s "$lib_path")
    fi
    size_mb=$(echo "scale=2; $size / 1024 / 1024" | bc)
    build_sizes[$arch]=$size_mb

    echo -e "${GREEN}✓ $arch complete${NC}"
    echo "  Library: $lib_path"
    echo "  Size: ${size_mb}MB"
    echo "  Build time: ${build_time}s"
    echo ""
done

# Summary
echo -e "${BLUE}=== Build Summary ===${NC}"
echo ""
printf "%-20s %-15s %-15s\n" "Architecture" "Size (MB)" "Build Time (s)"
echo "--------------------------------------------------------"

for arch in "${ARCHS[@]}"; do
    if [ -n "${build_sizes[$arch]}" ]; then
        printf "%-20s %-15s %-15s\n" "$arch" "${build_sizes[$arch]}" "${build_times[$arch]}"
    fi
done

echo ""
echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "Optimization tips:"
echo "  • For production: Use arm64-v8a (best performance)"
echo "  • For compatibility: Include armeabi-v7a (older devices)"
echo "  • Target-specific CPU: Edit .cargo/config.toml, change cortex-a53"
echo ""
echo "Verify symbols (should only show quiche_*):"
echo "  \$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/*/bin/llvm-nm -D target/aarch64-linux-android/release/libquiche.so | grep ' T '"
echo ""
