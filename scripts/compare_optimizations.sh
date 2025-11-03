#!/bin/bash

# Script to compare different optimization configurations for quiche
# Usage: ./scripts/compare_optimizations.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Result storage
declare -A sizes
declare -A times

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   quiche Optimization Comparison${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to build and measure
build_and_measure() {
    local name="$1"
    local profile="$2"
    local rustflags="$3"
    local features="$4"

    echo -e "${YELLOW}Building: $name${NC}"
    echo "Profile: $profile"
    echo "RUSTFLAGS: $rustflags"
    echo "Features: $features"

    # Clean previous build
    cargo clean -p quiche 2>/dev/null || true

    # Measure build time
    start_time=$(date +%s)

    if [ -n "$rustflags" ]; then
        RUSTFLAGS="$rustflags" cargo build --profile "$profile" --package quiche $features 2>&1 | tail -5
    else
        cargo build --profile "$profile" --package quiche $features 2>&1 | tail -5
    fi

    end_time=$(date +%s)
    build_time=$((end_time - start_time))

    # Get binary size (try multiple locations)
    binary=""
    if [ -f "target/$profile/libquiche.a" ]; then
        binary="target/$profile/libquiche.a"
    elif [ -f "target/release/libquiche.a" ]; then
        binary="target/release/libquiche.a"
    elif [ -f "target/debug/libquiche.a" ]; then
        binary="target/debug/libquiche.a"
    fi

    if [ -n "$binary" ] && [ -f "$binary" ]; then
        # Platform-specific stat command
        if [[ "$OSTYPE" == "darwin"* ]]; then
            size=$(stat -f%z "$binary")
        else
            size=$(stat -c%s "$binary")
        fi
        size_mb=$(echo "scale=2; $size / 1024 / 1024" | bc)
    else
        size_mb="N/A"
        size=0
    fi

    # Store results
    sizes["$name"]=$size
    times["$name"]=$build_time

    echo -e "${GREEN}✓ Build completed${NC}"
    echo -e "  Size: ${size_mb}MB"
    echo -e "  Time: ${build_time}s"
    echo ""
}

# Configuration 1: Default dev (baseline)
echo -e "${BLUE}=== Configuration 1: Default Dev (Baseline) ===${NC}"
build_and_measure "dev-default" "dev" "" ""

# Configuration 2: Default release (current)
echo -e "${BLUE}=== Configuration 2: Default Release (Current) ===${NC}"
build_and_measure "release-default" "release" "" ""

# Configuration 3: Size-optimized
echo -e "${BLUE}=== Configuration 3: Size Optimized ===${NC}"
build_and_measure "release-size" "release" "-C opt-level=z -C lto=fat -C codegen-units=1 -C strip=symbols -C panic=abort" ""

# Configuration 4: Performance-optimized
echo -e "${BLUE}=== Configuration 4: Performance Optimized ===${NC}"
build_and_measure "release-perf" "release" "-C opt-level=3 -C lto=fat -C codegen-units=1" ""

# Configuration 5: Balanced
echo -e "${BLUE}=== Configuration 5: Balanced ===${NC}"
build_and_measure "release-balanced" "release" "-C opt-level=2 -C lto=thin" ""

# Configuration 6: No default features (minimal)
echo -e "${BLUE}=== Configuration 6: Minimal Features ===${NC}"
build_and_measure "release-minimal" "release" "-C opt-level=z -C lto=fat -C codegen-units=1 -C strip=symbols" "--no-default-features --features boringssl-vendored"

# Print comparison table
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}         COMPARISON SUMMARY${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

printf "%-25s %-15s %-15s %-15s\n" "Configuration" "Size (MB)" "Build Time (s)" "Size vs Default"

# Calculate baseline (dev)
baseline_size=${sizes["dev-default"]}
release_size=${sizes["release-default"]}

for config in "dev-default" "release-default" "release-size" "release-perf" "release-balanced" "release-minimal"; do
    size=${sizes[$config]}
    time=${times[$config]}

    if [ "$size" -gt 0 ] && [ "$release_size" -gt 0 ]; then
        size_mb=$(echo "scale=2; $size / 1024 / 1024" | bc)
        reduction=$(echo "scale=1; ($release_size - $size) * 100 / $release_size" | bc)

        if (( $(echo "$reduction > 0" | bc -l) )); then
            reduction_str="${GREEN}-${reduction}%${NC}"
        elif (( $(echo "$reduction < 0" | bc -l) )); then
            reduction_str="${RED}+${reduction#-}%${NC}"
        else
            reduction_str="${YELLOW}0%${NC}"
        fi

        printf "%-25s %-15s %-15s " "$config" "${size_mb}" "${time}"
        echo -e "$reduction_str"
    else
        printf "%-25s %-15s %-15s %-15s\n" "$config" "N/A" "$time" "N/A"
    fi
done

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}         RECOMMENDATIONS${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Find best configurations
min_size_config=""
min_size=999999999999
min_time_config=""
min_time=999999
best_balanced=""
best_balanced_score=999999

for config in "release-size" "release-perf" "release-balanced" "release-minimal"; do
    size=${sizes[$config]}
    time=${times[$config]}

    if [ "$size" -gt 0 ]; then
        if [ "$size" -lt "$min_size" ]; then
            min_size=$size
            min_size_config=$config
        fi
    fi

    if [ "$time" -lt "$min_time" ] && [ "$config" != "dev-default" ]; then
        min_time=$time
        min_time_config=$config
    fi

    # Balanced score: size (MB) + time (minutes)
    score=$(echo "scale=2; $size / 1024 / 1024 + $time / 60" | bc)
    if (( $(echo "$score < $best_balanced_score" | bc -l) )); then
        best_balanced_score=$score
        best_balanced=$config
    fi
done

echo -e "${GREEN}Smallest binary:${NC} $min_size_config ($(echo "scale=2; $min_size / 1024 / 1024" | bc)MB)"
echo -e "${GREEN}Fastest build:${NC} $min_time_config (${times[$min_time_config]}s)"
echo -e "${GREEN}Best balanced:${NC} $best_balanced (score: $best_balanced_score)"

echo ""
echo -e "${YELLOW}To apply the recommended optimizations:${NC}"
echo "1. Update Cargo.toml with the settings from Cargo.toml.optimized.example"
echo "2. Copy .cargo/config.toml.example to .cargo/config.toml"
echo "3. Build with: cargo build --profile release-balanced --package quiche"

echo ""
echo -e "${BLUE}========================================${NC}"
