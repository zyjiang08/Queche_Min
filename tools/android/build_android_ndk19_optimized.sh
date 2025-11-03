#!/bin/sh
#
# Build quiche for Android NDK 19 or higher with OPTIMIZATIONS
#
# This is an optimized version of the original build_android_ndk19.sh
# It uses the optimizations defined in .cargo/config.toml
#
# ANDROID_NDK_HOME : android ndk location
# TOOLCHAIN_DIR : where create a toolchain (optional)
#
# Expected size reduction: 65-70% (from 28MB to 8-10MB per arch)
#
set -eu

# Change this value if you need a different API level
# 21 is the minimum API tested
API_LEVEL=21

if [ ! -d "${ANDROID_NDK_HOME-}" ]; then
    ANDROID_NDK_HOME=/usr/local/share/android-ndk
fi

if [ ! -d "${TOOLCHAIN_DIR-}" ]; then
    TOOLCHAIN_DIR=$(pwd)
fi

echo "> building OPTIMIZED quiche for android API $API_LEVEL..."
echo "> optimizations: opt-level=z, lto=fat, gc-sections, strip-all"
echo "> see .cargo/config.toml for full details"
echo ""

for arch in arm64-v8a armeabi-v7a x86_64 x86
do
    echo "> building $arch..."

    # Build with optimizations from .cargo/config.toml
    # Use --no-default-features to minimize size
    cargo ndk -t $arch -p $API_LEVEL -- build \
        --release \
        --no-default-features \
        --features ffi,boringssl-vendored \
        $*

    # Map architecture to target triple
    case $arch in
        arm64-v8a)
            target="aarch64-linux-android"
            ;;
        armeabi-v7a)
            target="armv7-linux-androideabi"
            ;;
        x86_64)
            target="x86_64-linux-android"
            ;;
        x86)
            target="i686-linux-android"
            ;;
    esac

    # Find and report library size
    lib="target/$target/release/libquiche.so"
    if [ -f "$lib" ]; then
        size=$(ls -lh "$lib" | awk '{print $5}')
        echo "  └─ size: $size"
    fi

    lib_a="target/$target/release/libquiche.a"
    if [ -f "$lib_a" ]; then
        size=$(ls -lh "$lib_a" | awk '{print $5}')
        echo "  └─ static lib size: $size"
    fi

    echo ""
done

echo "> build complete!"
echo ""
echo "To verify optimizations:"
echo "  • Check size: ls -lh target/*/release/libquiche.*"
echo "  • Check symbols: \$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/*/bin/llvm-nm -D target/aarch64-linux-android/release/libquiche.so"
echo ""
