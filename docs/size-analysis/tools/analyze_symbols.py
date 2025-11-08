#!/usr/bin/env python3
"""
Analyze libquiche_engine.so symbols to determine size contribution from each component.
Uses llvm-nm output to categorize symbols by source: BoringSSL, libev, Rust quiche, C++ engine.
"""

import re
import sys
import subprocess
from collections import defaultdict

def parse_hex_size(size_hex):
    """Convert hexadecimal size string to integer bytes."""
    try:
        return int(size_hex, 16)
    except ValueError:
        return 0

def categorize_symbol(symbol_name):
    """Categorize a symbol based on its name pattern."""

    # BoringSSL patterns (C functions, unmangled names)
    boringssl_prefixes = [
        'RSA_', 'DSA_', 'DH_', 'EC_', 'ECDSA_', 'ECDH_',
        'EVP_', 'SSL_', 'TLS_', 'DTLS_',
        'BN_', 'bn_', 'BIO_', 'ASN1_', 'asn1_',
        'X509_', 'x509_', 'X509V3_', 'x509v3_',
        'OPENSSL_', 'openssl_', 'CRYPTO_', 'crypto_',
        'AES_', 'aes_', 'ChaCha20_', 'chacha', 'Poly1305_',
        'SHA', 'sha', 'MD5_', 'md5_', 'HMAC_',
        'RAND_', 'rand_', 'PEM_', 'pem_',
        'PKCS', 'pkcs', 'OBJ_', 'obj_',
        'DES_', 'des_', 'RC4_', 'ED25519_', 'x25519_',
        'TRUST_TOKEN_', 'pmbtoken_', 'voprf_',
        'boringssl_', 'fips_', 'fiat_p256_',
        'ec_GFp_', 'get_crl_', 'def_load_',
        'r2i_', 'i2r_', 'generate_v3'
    ]

    for prefix in boringssl_prefixes:
        if symbol_name.startswith(prefix):
            return 'boringssl'

    # BoringSSL namespace (C++ mangled names)
    if '_ZN4bssl' in symbol_name:
        return 'boringssl'

    # libev patterns
    if symbol_name.startswith('ev_') or '_ev_' in symbol_name or 'libev' in symbol_name.lower():
        return 'libev'

    # C++ engine patterns
    if ('quiche_engine' in symbol_name.lower() or
        '_ZN6quiche12QuicheEngine' in symbol_name or
        '_ZN6quiche16QuicheEngineImpl' in symbol_name or
        '_ZN6quiche12CommandQueue' in symbol_name or
        '_ZN6quiche12thread_utils' in symbol_name):
        return 'cpp_engine'

    # Rust quiche QUIC protocol
    if '_ZN6quiche' in symbol_name:
        return 'quiche_rust'

    # Rust standard library and debugging crates
    if any(pattern in symbol_name for pattern in [
        '_ZN4core', '_ZN3std', '_ZN5alloc',
        '_ZN9addr2line', '_ZN5gimli', '_ZN9libunwind',
        '_ZN14rustc_demangle', '_ZN11miniz_oxide'
    ]):
        return 'rust_stdlib'

    # System/C++ stdlib
    if any(pattern in symbol_name for pattern in [
        '_ZNSt', '_ZN', '__cxa_', '__gxx_',
        'pthread_', '_GLOBAL_', 'vtable', 'typeinfo'
    ]):
        return 'system'

    # Data segments
    if symbol_name.startswith('k') and symbol_name[1].isupper():
        # Likely BoringSSL constants (kOpenSSL*, kPrimes, kObjects, etc.)
        return 'boringssl'

    return 'unknown'

def analyze_symbols(so_path, nm_path):
    """Parse nm output and categorize symbols by component."""

    components = defaultdict(int)
    symbol_details = defaultdict(list)
    total_size = 0

    # Run llvm-nm with size information
    cmd = [nm_path, '-S', '--size-sort', so_path]
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"Error running nm: {result.stderr}")
        return None, None, 0

    # Parse nm output
    # Format: address size type symbol_name
    for line in result.stdout.split('\n'):
        parts = line.split()
        if len(parts) < 4:
            continue

        # Extract size (2nd column) and symbol name (4th+ column)
        size = parse_hex_size(parts[1])
        symbol_name = ' '.join(parts[3:])

        if size == 0:
            continue

        # Categorize
        component = categorize_symbol(symbol_name)

        # Accumulate
        components[component] += size
        total_size += size

        # Store large symbols for details (> 1KB)
        if size > 1024:
            symbol_details[component].append({
                'size': size,
                'name': symbol_name[:100]  # Truncate long names
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

    print("=" * 90)
    print("libquiche_engine.so - Size Composition Analysis (Based on Symbol Names)")
    print("=" * 90)
    print()

    # Sort components by size (descending)
    sorted_components = sorted(components.items(), key=lambda x: x[1], reverse=True)

    print(f"{'Component':<20} {'Size':>15} {'Percentage':>12} {'Description':<40}")
    print("-" * 90)

    component_desc = {
        'boringssl': 'BoringSSL (SSL/TLS + crypto)',
        'quiche_rust': 'Rust QUIC protocol impl',
        'rust_stdlib': 'Rust stdlib + debug crates',
        'cpp_engine': 'C++ wrapper engine',
        'libev': 'Event loop library',
        'system': 'C++ stdlib + system',
        'unknown': 'Unclassified symbols'
    }

    for component, size in sorted_components:
        percentage = (size / total_size * 100) if total_size > 0 else 0
        desc = component_desc.get(component, '')
        print(f"{component:<20} {format_size(size):>15} {percentage:>11.2f}% {desc:<40}")

    print("-" * 90)
    print(f"{'TOTAL':<20} {format_size(total_size):>15} {'100.00%':>12}")
    print()

    # Core components summary
    print("=" * 90)
    print("CORE COMPONENTS SUMMARY")
    print("=" * 90)

    boringssl_size = components.get('boringssl', 0)
    quiche_size = components.get('quiche_rust', 0)
    rust_stdlib_size = components.get('rust_stdlib', 0)
    cpp_size = components.get('cpp_engine', 0)
    libev_size = components.get('libev', 0)

    core_total = boringssl_size + quiche_size + rust_stdlib_size

    print(f"1. BoringSSL:           {format_size(boringssl_size):>12} ({boringssl_size / total_size * 100:>5.1f}%)")
    print(f"2. Rust QUIC:           {format_size(quiche_size):>12} ({quiche_size / total_size * 100:>5.1f}%)")
    print(f"3. Rust Stdlib:         {format_size(rust_stdlib_size):>12} ({rust_stdlib_size / total_size * 100:>5.1f}%)")
    print(f"   ----------------------------------------")
    print(f"   Core (1+2+3):        {format_size(core_total):>12} ({core_total / total_size * 100:>5.1f}%)")
    print()
    print(f"4. C++ Engine:          {format_size(cpp_size):>12} ({cpp_size / total_size * 100:>5.1f}%)")
    print(f"5. libev:               {format_size(libev_size):>12} ({libev_size / total_size * 100:>5.1f}%)")
    print()

    # Detailed breakdown for main components
    main_components = ['boringssl', 'quiche_rust', 'rust_stdlib', 'cpp_engine']

    for component in main_components:
        if component not in components:
            continue

        print("=" * 90)
        print(f"{component.upper().replace('_', ' ')} - Top 30 Largest Symbols")
        print("=" * 90)

        # Sort symbols by size
        symbols = sorted(symbol_details[component], key=lambda x: x['size'], reverse=True)[:30]

        if not symbols:
            print("  (No detailed symbols found)")
            print()
            continue

        print(f"{'Size':>12}  {'Symbol'}")
        print("-" * 90)

        for sym in symbols:
            print(f"{format_size(sym['size']):>12}  {sym['name']}")

        print()

    # Optimization recommendations
    print("=" * 90)
    print("优化建议 / OPTIMIZATION RECOMMENDATIONS")
    print("=" * 90)
    print()

    boringssl_pct = boringssl_size / total_size * 100
    quiche_pct = quiche_size / total_size * 100
    rust_stdlib_pct = rust_stdlib_size / total_size * 100

    print(f"1. BoringSSL占比: {boringssl_pct:.1f}% ({format_size(boringssl_size)})")
    if boringssl_pct > 30:
        print("   ✓ 已经通过深度裁剪优化 (禁用了30+个不需要的算法和协议)")
        print("   - 进一步优化空间有限，除非禁用更多加密算法")
    else:
        print("   ✓ BoringSSL占比合理")
    print()

    print(f"2. Rust QUIC占比: {quiche_pct:.1f}% ({format_size(quiche_size)})")
    print("   ✓ QUIC协议核心功能，占比符合预期")
    print("   - 已使用LTO+opt-level=z优化")
    print()

    print(f"3. Rust Stdlib占比: {rust_stdlib_pct:.1f}% ({format_size(rust_stdlib_size)})")
    if rust_stdlib_pct > 15:
        print("   ! 包含调试符号相关代码 (addr2line, gimli, libunwind)")
        print("   建议: Release构建时可以禁用backtrace功能进一步减小")
    print()

    libev_pct = libev_size / total_size * 100 if libev_size > 0 else 0
    if libev_size > 0:
        print(f"4. libev占比: {libev_pct:.1f}% ({format_size(libev_size)})")
        print("   ✓ 事件循环库，体积很小")
        print()

    cpp_pct = cpp_size / total_size * 100 if cpp_size > 0 else 0
    if cpp_size > 0:
        print(f"5. C++ Engine占比: {cpp_pct:.1f}% ({format_size(cpp_size)})")
        print("   ✓ C++包装层，体积很小")
        print()

    print("=" * 90)
    print("总结 / SUMMARY")
    print("=" * 90)
    print()
    print(f"当前库大小: {format_size(total_size)} (分析的符号大小)")
    print(f"主要组成:")
    print(f"  - BoringSSL (SSL/TLS + 加密): {boringssl_pct:.1f}%")
    print(f"  - Rust QUIC协议实现:          {quiche_pct:.1f}%")
    print(f"  - Rust标准库+调试工具:        {rust_stdlib_pct:.1f}%")
    print()
    print("优化状态: ✅ 已深度优化")
    print("- LTO: Thin LTO enabled")
    print("- Optimization level: opt-level=\"z\" (size)")
    print("- BoringSSL: 30+ features disabled")
    print("- Panic mode: abort (reduced unwinding code)")
    print()
    print("进一步优化建议:")
    print("1. 如需更小体积，可考虑禁用backtrace功能 (会影响调试体验)")
    print("2. BoringSSL已深度裁剪，进一步优化收益有限")
    print()

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 analyze_symbols.py <libquiche_engine.so> <llvm-nm path>")
        sys.exit(1)

    so_path = sys.argv[1]
    nm_path = sys.argv[2]

    print(f"Analyzing symbols from: {so_path}")
    print(f"Using nm: {nm_path}")
    print()

    components, symbol_details, total_size = analyze_symbols(so_path, nm_path)

    if components is None:
        sys.exit(1)

    print_report(components, symbol_details, total_size)

if __name__ == '__main__':
    main()
