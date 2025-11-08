#!/usr/bin/env python3
"""
Analyze Android link map to determine size contribution from each component.
Categorizes symbols by source: BoringSSL, libev, Rust quiche, C++ engine.
"""

import re
import sys
from collections import defaultdict

def parse_size(size_hex):
    """Convert hexadecimal size string to integer bytes."""
    if not size_hex or size_hex.strip() == '':
        return 0
    try:
        return int(size_hex.strip(), 16)
    except ValueError:
        return 0

def categorize_component(in_field):
    """Categorize a symbol based on its source file path."""
    if not in_field:
        return 'unknown'

    in_lower = in_field.lower()

    # BoringSSL patterns
    boringssl_patterns = [
        'libcrypto.a', 'libssl.a',
        '/ssl/', '/crypto/',
        'boringssl', 'boring'
    ]
    if any(pattern in in_lower for pattern in boringssl_patterns):
        # Exclude Rust's boring crate
        if 'libquiche.a' not in in_field and 'boring-' not in in_lower:
            return 'boringssl'

    # libev patterns
    if 'libev.a' in in_lower or '/ev.c.o' in in_lower or 'deps/libev' in in_lower:
        return 'libev'

    # Rust quiche patterns
    if 'libquiche.a' in in_field:
        return 'quiche_rust'

    # C++ engine patterns
    if 'libquiche_engine.a' in in_field or any(pattern in in_lower for pattern in [
        'quiche_engine_api', 'quiche_engine_impl', 'quiche_thread_utils'
    ]):
        return 'cpp_engine'

    # System libraries
    if any(pattern in in_lower for pattern in [
        'crtbegin', 'crtend', 'sysroot', '<internal>', 'libc++', 'android'
    ]):
        return 'system'

    return 'unknown'

def analyze_linkmap(linkmap_path):
    """Parse linkmap file and categorize symbols by component."""

    components = defaultdict(int)
    symbol_details = defaultdict(list)
    total_size = 0

    with open(linkmap_path, 'r', encoding='utf-8', errors='ignore') as f:
        # Skip header line
        header = f.readline()

        for line_num, line in enumerate(f, start=2):
            # Parse line: VMA LMA Size Align Out In Symbol
            parts = line.split(maxsplit=6)

            if len(parts) < 3:
                continue

            # Extract size (3rd column)
            size = parse_size(parts[2])
            if size == 0:
                continue

            # Extract "In" field (6th column if exists)
            in_field = parts[5] if len(parts) >= 6 else ''

            # Categorize component
            component = categorize_component(in_field)

            # Accumulate size
            components[component] += size
            total_size += size

            # Store details for top symbols (only if size > 1KB)
            if size > 1024:
                symbol_name = parts[6].strip() if len(parts) >= 7 else '(unnamed)'
                symbol_details[component].append({
                    'size': size,
                    'name': symbol_name[:80],  # Truncate long names
                    'source': in_field[:100]
                })

    return components, symbol_details, total_size

def format_size(bytes_val):
    """Format byte size as human-readable string."""
    if bytes_val < 1024:
        return f"{bytes_val} B"
    elif bytes_val < 1024 * 1024:
        return f"{bytes_val / 1024:.2f} KB"
    else:
        return f"{bytes_val / (1024 * 1024):.2f} MB"

def print_report(components, symbol_details, total_size):
    """Print detailed size breakdown report."""

    print("=" * 80)
    print("libquiche_engine.so - Size Composition Analysis")
    print("=" * 80)
    print()

    # Sort components by size (descending)
    sorted_components = sorted(components.items(), key=lambda x: x[1], reverse=True)

    print(f"{'Component':<20} {'Size':>15} {'Percentage':>12}")
    print("-" * 80)

    for component, size in sorted_components:
        percentage = (size / total_size * 100) if total_size > 0 else 0
        print(f"{component:<20} {format_size(size):>15} {percentage:>11.2f}%")

    print("-" * 80)
    print(f"{'TOTAL':<20} {format_size(total_size):>15} {'100.00%':>12}")
    print()

    # Detailed breakdown for main components
    main_components = ['boringssl', 'quiche_rust', 'libev', 'cpp_engine']

    for component in main_components:
        if component not in components:
            continue

        print("=" * 80)
        print(f"{component.upper()} - Top 20 Largest Symbols")
        print("=" * 80)

        # Sort symbols by size
        symbols = sorted(symbol_details[component], key=lambda x: x['size'], reverse=True)[:20]

        if not symbols:
            print("  (No detailed symbols found)")
            print()
            continue

        print(f"{'Size':>12}  {'Symbol'}")
        print("-" * 80)

        for sym in symbols:
            print(f"{format_size(sym['size']):>12}  {sym['name']}")

        print()

    # Summary for optimization recommendations
    print("=" * 80)
    print("OPTIMIZATION RECOMMENDATIONS")
    print("=" * 80)
    print()

    boringssl_size = components.get('boringssl', 0)
    quiche_size = components.get('quiche_rust', 0)

    if boringssl_size > 0:
        boringssl_pct = boringssl_size / total_size * 100
        print(f"1. BoringSSL占比: {boringssl_pct:.1f}% ({format_size(boringssl_size)})")
        if boringssl_pct > 30:
            print("   建议: BoringSSL占比较高，可以考虑进一步裁剪不需要的加密算法")
        else:
            print("   建议: BoringSSL占比合理，已经过深度优化")

    if quiche_size > 0:
        quiche_pct = quiche_size / total_size * 100
        print(f"2. Rust QUIC占比: {quiche_pct:.1f}% ({format_size(quiche_size)})")
        if quiche_pct > 50:
            print("   建议: Rust QUIC是核心功能，占比符合预期")

    libev_size = components.get('libev', 0)
    if libev_size > 0:
        libev_pct = libev_size / total_size * 100
        print(f"3. libev占比: {libev_pct:.1f}% ({format_size(libev_size)})")

    cpp_size = components.get('cpp_engine', 0)
    if cpp_size > 0:
        cpp_pct = cpp_size / total_size * 100
        print(f"4. C++ Engine占比: {cpp_pct:.1f}% ({format_size(cpp_size)})")

    print()

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_linkmap.py <linkmap.txt>")
        sys.exit(1)

    linkmap_path = sys.argv[1]

    print(f"Analyzing link map: {linkmap_path}")
    print()

    components, symbol_details, total_size = analyze_linkmap(linkmap_path)
    print_report(components, symbol_details, total_size)

if __name__ == '__main__':
    main()
