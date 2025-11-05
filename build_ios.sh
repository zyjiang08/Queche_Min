#!/bin/bash
# build_ios.sh - Build quiche for iOS platforms
# This script builds quiche for all iOS architectures

set -e

echo "=========================================="
echo "Building quiche for iOS"
echo "=========================================="

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check if Rust is installed
if ! command -v cargo &> /dev/null; then
    echo -e "${RED}Error: Rust/Cargo not found. Please install Rust first.${NC}"
    echo "Visit: https://rustup.rs/"
    exit 1
fi

# Check if cmake is installed
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake not found. Please install cmake first.${NC}"
    echo "Run: brew install cmake"
    exit 1
fi

# Function to check and install iOS target
check_target() {
    local target=$1
    if ! rustup target list --installed | grep -q "$target"; then
        echo -e "${BLUE}Installing $target...${NC}"
        rustup target add "$target"
    else
        echo -e "${GREEN}✓ $target already installed${NC}"
    fi
}

# Install iOS targets
echo -e "\n${BLUE}Checking iOS targets...${NC}"
check_target "aarch64-apple-ios"           # iOS devices (ARM64)
check_target "aarch64-apple-ios-sim"       # iOS simulator (ARM64, M1+ Macs)
check_target "x86_64-apple-ios"            # iOS simulator (Intel Macs)

# Clean previous builds
echo -e "\n${BLUE}Cleaning previous builds...${NC}"
cargo clean

# Build for iOS device (ARM64)
echo -e "\n${BLUE}=========================================="
echo "Building for iOS Device (ARM64)"
echo -e "==========================================${NC}"
cargo build --release \
    --target aarch64-apple-ios \
    --no-default-features \
    --features ffi,boringssl-vendored

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ iOS Device build successful${NC}"
    ls -lh target/aarch64-apple-ios/release/libquiche.a
else
    echo -e "${RED}✗ iOS Device build failed${NC}"
    exit 1
fi

# Build for iOS Simulator (ARM64, for M1+ Macs)
echo -e "\n${BLUE}=========================================="
echo "Building for iOS Simulator (ARM64)"
echo -e "==========================================${NC}"
cargo build --release \
    --target aarch64-apple-ios-sim \
    --no-default-features \
    --features ffi,boringssl-vendored

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ iOS Simulator ARM64 build successful${NC}"
    ls -lh target/aarch64-apple-ios-sim/release/libquiche.a
else
    echo -e "${RED}✗ iOS Simulator ARM64 build failed${NC}"
    exit 1
fi

# Build for iOS Simulator (x86_64, for Intel Macs)
echo -e "\n${BLUE}=========================================="
echo "Building for iOS Simulator (x86_64)"
echo -e "==========================================${NC}"
cargo build --release \
    --target x86_64-apple-ios \
    --no-default-features \
    --features ffi,boringssl-vendored

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ iOS Simulator x86_64 build successful${NC}"
    ls -lh target/x86_64-apple-ios/release/libquiche.a
else
    echo -e "${RED}✗ iOS Simulator x86_64 build failed${NC}"
    exit 1
fi

# Create output directory
OUTPUT_DIR="ios-libs"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# Copy libraries
echo -e "\n${BLUE}Copying libraries to $OUTPUT_DIR...${NC}"
cp target/aarch64-apple-ios/release/libquiche.a "$OUTPUT_DIR/libquiche-ios-arm64.a"
cp target/aarch64-apple-ios-sim/release/libquiche.a "$OUTPUT_DIR/libquiche-sim-arm64.a"
cp target/x86_64-apple-ios/release/libquiche.a "$OUTPUT_DIR/libquiche-sim-x86_64.a"

# Create universal simulator library
echo -e "\n${BLUE}Creating universal simulator library...${NC}"
lipo -create \
    "$OUTPUT_DIR/libquiche-sim-arm64.a" \
    "$OUTPUT_DIR/libquiche-sim-x86_64.a" \
    -output "$OUTPUT_DIR/libquiche-simulator.a"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Universal simulator library created${NC}"
    ls -lh "$OUTPUT_DIR/libquiche-simulator.a"
fi

# Create XCFramework
echo -e "\n${BLUE}Creating XCFramework...${NC}"
rm -rf libquiche.xcframework

xcodebuild -create-xcframework \
    -library "$OUTPUT_DIR/libquiche-ios-arm64.a" \
    -library "$OUTPUT_DIR/libquiche-simulator.a" \
    -output libquiche.xcframework

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ XCFramework created successfully${NC}"
    echo -e "\n${BLUE}XCFramework contents:${NC}"
    ls -lh libquiche.xcframework
fi

# Verify symbols
echo -e "\n${BLUE}Verifying __chkstk_darwin symbol...${NC}"
if nm -g "$OUTPUT_DIR/libquiche-ios-arm64.a" 2>/dev/null | grep -q "__chkstk_darwin"; then
    echo -e "${GREEN}✓ __chkstk_darwin symbol found${NC}"
    nm -g "$OUTPUT_DIR/libquiche-ios-arm64.a" | grep "__chkstk_darwin"
else
    echo -e "${RED}⚠ __chkstk_darwin symbol not found (this is OK if not needed)${NC}"
fi

# Check exported quiche functions
echo -e "\n${BLUE}Checking quiche exported functions...${NC}"
nm -g "$OUTPUT_DIR/libquiche-ios-arm64.a" | grep " T _quiche" | head -10

# Summary
echo -e "\n${GREEN}=========================================="
echo "Build Summary"
echo -e "==========================================${NC}"
echo -e "✓ iOS Device (ARM64):    ${OUTPUT_DIR}/libquiche-ios-arm64.a"
echo -e "✓ Simulator (Universal): ${OUTPUT_DIR}/libquiche-simulator.a"
echo -e "✓ XCFramework:           libquiche.xcframework/"
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "1. Add libquiche.xcframework to your Xcode project"
echo "2. Link against Security.framework and libresolv.tbd"
echo "3. Add quiche/include to Header Search Paths"
echo ""
echo -e "${GREEN}Build completed successfully!${NC}"
