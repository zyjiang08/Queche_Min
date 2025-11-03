#!/bin/bash
# Generate symbol export lists for Android and iOS
# This extracts all quiche_* public functions from FFI code

set -e

echo "=== Generating Symbol Export Lists ==="
echo ""

# Check if source files exist
if [ ! -f "quiche/src/ffi.rs" ]; then
    echo "Error: quiche/src/ffi.rs not found"
    echo "Please run this script from the quiche root directory"
    exit 1
fi

# Create output directory
mkdir -p quiche

# Extract quiche_* functions from ffi.rs
echo "Scanning quiche/src/ffi.rs..."
quiche_main=$(grep -h '#\[no_mangle\]' quiche/src/ffi.rs 2>/dev/null | \
    grep -A 1 'pub extern "C"' | \
    grep 'fn quiche_' | \
    sed 's/.*fn \([a-zA-Z0-9_]*\).*/\1/' | \
    sort -u)

# Also check h3/ffi.rs if it exists
quiche_h3=""
if [ -f "quiche/src/h3/ffi.rs" ]; then
    echo "Scanning quiche/src/h3/ffi.rs..."
    quiche_h3=$(grep -h '#\[no_mangle\]' quiche/src/h3/ffi.rs 2>/dev/null | \
        grep -A 1 'pub extern "C"' | \
        grep 'fn quiche_' | \
        sed 's/.*fn \([a-zA-Z0-9_]*\).*/\1/' | \
        sort -u)
fi

# Combine and deduplicate
all_symbols=$(echo -e "$quiche_main\n$quiche_h3" | sort -u | grep -v '^$')

# Generate Android/Linux export list (no underscore prefix)
echo "$all_symbols" > quiche/exports_android.txt
echo "✓ Created: quiche/exports_android.txt"
echo "  Symbols: $(wc -l < quiche/exports_android.txt)"

# Generate iOS/macOS export list (with underscore prefix)
echo "$all_symbols" | sed 's/^/_/' > quiche/exports_ios.txt
echo "✓ Created: quiche/exports_ios.txt"
echo "  Symbols: $(wc -l < quiche/exports_ios.txt)"

echo ""
echo "=== Sample Symbols ==="
echo "First 10 functions:"
head -10 quiche/exports_android.txt

echo ""
echo "=== Usage ==="
echo ""
echo "Android/Linux:"
echo "  Add to .cargo/config.toml:"
echo "  rustflags = [\"-C\", \"link-arg=-Wl,--dynamic-list=quiche/exports_android.txt\"]"
echo ""
echo "iOS/macOS:"
echo "  Add to .cargo/config.toml:"
echo "  rustflags = [\"-C\", \"link-arg=-Wl,-exported_symbols_list,quiche/exports_ios.txt\"]"
echo ""

echo "Done!"
