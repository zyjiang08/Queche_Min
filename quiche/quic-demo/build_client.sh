#!/bin/bash

# QUIC Client Build Script
# Supports: iOS, Android, macOS, Linux (multiple architectures)

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Show usage
show_usage() {
    info "============================================"
    info "QUIC Client Build Script"
    info "============================================"
    info ""
    info "Usage: $0 <platform> [arch] [<platform> [arch] ...]"
    info ""
    info "Platforms:"
    info "  ios [arch]          - Build for iOS"
    info "  macos [arch]        - Build for macOS"
    info "  android [arch]      - Build for Android"
    info "  linux [arch]        - Build for Linux"
    info "  all                 - Build for all platforms (current host only)"
    info ""
    info "Architectures:"
    info "  iOS:     arm64 (device), x86_64 (simulator), all"
    info "  macOS:   arm64 (Apple Silicon), x86_64 (Intel), all"
    info "  Android: arm64-v8a, armeabi-v7a, x86, x86_64, all"
    info "  Linux:   arm64, x86_64, all"
    info ""
    info "If [arch] is omitted:"
    info "  ios      - builds arm64 (device)"
    info "  macos    - builds current architecture"
    info "  android  - builds all architectures"
    info "  linux    - builds current architecture"
    info ""
    info "Examples:"
    info "  $0 ios arm64                    # iOS device"
    info "  $0 ios x86_64                   # iOS simulator"
    info "  $0 ios all                      # All iOS architectures"
    info "  $0 macos arm64                  # macOS Apple Silicon"
    info "  $0 macos all                    # All macOS architectures"
    info "  $0 android arm64-v8a            # Android ARM64"
    info "  $0 android all                  # All Android architectures"
    info "  $0 ios arm64 android arm64-v8a  # iOS + Android"
    info "  $0 all                          # All platforms (host only)"
    info ""
    info "Prerequisites:"
    info "  1. quiche_engine library must be built first:"
    info "     cd ../.. && ./quiche_engine_all.sh <platform> <arch>"
    info ""
    info "  2. Or build both quiche_engine and client together:"
    info "     cd ../.. && ./quiche_engine_all.sh <platform> <arch>"
    info "     cd quiche/quic-demo && ./build_client.sh <platform> <arch>"
}

# Build quiche_engine if needed
ensure_quiche_engine() {
    local platform=$1
    local arch=$2

    local lib_path="../../lib/${platform}/${arch}/libquiche_engine.a"

    if [ ! -f "$lib_path" ]; then
        warn "quiche_engine library not found: $lib_path"
        info "Building quiche_engine for ${platform} ${arch}..."

        cd ../..
        ./quiche_engine_all.sh "$platform" "$arch"
        cd quiche/quic-demo

        if [ ! -f "$lib_path" ]; then
            error "Failed to build quiche_engine library"
        fi
    else
        info "Using existing quiche_engine library: $lib_path"
    fi
}

# Build client for specific platform/arch
build_client() {
    local platform=$1
    local arch=$2

    info "======================================"
    info "Building client for ${platform} (${arch})..."
    info "======================================"

    # Ensure quiche_engine is built
    ensure_quiche_engine "$platform" "$arch"

    # Build using Makefile.client
    make -f Makefile.client PLATFORM="$platform" ARCH="$arch" clean-target
    make -f Makefile.client PLATFORM="$platform" ARCH="$arch"

    local output="quic-client-${platform}-${arch}"
    if [ -f "$output" ]; then
        info "✓ ${platform} ${arch} build successful"
        info "  Output: $output"
        info "  Size: $(du -h "$output" | cut -f1)"
    else
        error "Build failed: $output not found"
    fi
}

# Build for iOS
build_ios() {
    local arch=${1:-arm64}

    if [ "$arch" = "all" ]; then
        build_client "ios" "arm64"
        build_client "ios" "x86_64"
    else
        build_client "ios" "$arch"
    fi
}

# Build for macOS
build_macos() {
    local arch=${1:-$(uname -m)}

    if [ "$arch" = "all" ]; then
        build_client "macos" "arm64"
        build_client "macos" "x86_64"
    else
        build_client "macos" "$arch"
    fi
}

# Build for Android
build_android() {
    local arch=${1:-all}

    if [ "$arch" = "all" ]; then
        build_client "android" "arm64-v8a"
        build_client "android" "armeabi-v7a"
        build_client "android" "x86"
        build_client "android" "x86_64"
    else
        build_client "android" "$arch"
    fi
}

# Build for Linux
build_linux() {
    local arch=${1:-$(uname -m)}

    if [ "$arch" = "all" ]; then
        build_client "linux" "arm64"
        build_client "linux" "x86_64"
    else
        build_client "linux" "$arch"
    fi
}

# Main script
main() {
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi

    info "============================================"
    info "QUIC Client Build Script"
    info "============================================"
    info ""

    # Parse build plan
    local build_plan=()
    while [ $# -gt 0 ]; do
        case "$1" in
            ios|macos|android|linux)
                local platform=$1
                shift

                if [ $# -gt 0 ] && [[ ! "$1" =~ ^(ios|macos|android|linux|all)$ ]]; then
                    local arch=$1
                    shift
                else
                    local arch=""
                fi

                build_plan+=("$platform:$arch")
                ;;
            all)
                # Build for current host platform only
                local host_platform=""
                if [[ "$OSTYPE" == "darwin"* ]]; then
                    host_platform="macos"
                elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
                    host_platform="linux"
                else
                    error "Unsupported host platform: $OSTYPE"
                fi
                build_plan+=("$host_platform:")
                shift
                ;;
            -h|--help|help)
                show_usage
                exit 0
                ;;
            *)
                error "Unknown argument: $1"
                show_usage
                exit 1
                ;;
        esac
    done

    # Show build plan
    info "Build plan:"
    for item in "${build_plan[@]}"; do
        local platform="${item%:*}"
        local arch="${item#*:}"
        if [ -z "$arch" ]; then
            arch="(default)"
        fi
        info "  ${platform}: ${arch}"
    done
    info ""

    # Execute builds
    for item in "${build_plan[@]}"; do
        local platform="${item%:*}"
        local arch="${item#*:}"

        case "$platform" in
            ios)
                build_ios "$arch"
                ;;
            macos)
                build_macos "$arch"
                ;;
            android)
                build_android "$arch"
                ;;
            linux)
                build_linux "$arch"
                ;;
            *)
                error "Unsupported platform: $platform"
                ;;
        esac
    done

    info ""
    info "============================================"
    info "Build completed successfully!"
    info "============================================"
    info ""
    info "Built binaries:"
    ls -lh quic-client-* 2>/dev/null || info "  (none)"
    info ""
}

# Run main
main "$@"
