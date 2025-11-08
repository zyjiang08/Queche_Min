#!/usr/bin/env python3
"""
详细的Link Map分析工具 - 精确到.o文件级别
分析libquiche_engine.so的链接组成，统计每个.o文件的贡献
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

def parse_size(size_hex):
    """转换十六进制大小为字节"""
    if not size_hex or size_hex.strip() == '':
        return 0
    try:
        return int(size_hex.strip(), 16)
    except ValueError:
        return 0

def extract_object_file(in_field):
    """从输入字段提取.o文件名"""
    if not in_field:
        return (None, None)

    # 匹配 .a(...) 或 .o 文件
    # 例如: libquiche.a(quiche-xxx.o):(.text)
    # 或: libev.a(554095ed4a794b0a-ev.o):(.text)

    # 提取.a文件和.o文件
    match = re.search(r'([^/]+\.a)\(([^)]+)\)', in_field)
    if match:
        archive = match.group(1)
        obj_file = match.group(2)
        return (archive, obj_file)

    # 直接的.o文件
    match = re.search(r'([^/]+\.o)', in_field)
    if match:
        obj_file = match.group(1)
        return ('standalone', obj_file)

    return (None, None)

def categorize_component(archive_name, obj_file):
    """根据archive和object文件名分类"""
    if not archive_name:
        return 'system'

    archive_lower = archive_name.lower()

    # BoringSSL (libcrypto.a, libssl.a)
    if 'libcrypto.a' in archive_lower or 'libssl.a' in archive_lower:
        return 'boringssl'

    # libev
    if 'libev.a' in archive_lower:
        return 'libev'

    # C++ Engine
    if 'libquiche_engine.a' in archive_lower:
        return 'cpp_engine'

    # Rust quiche (包含BoringSSL)
    if 'libquiche.a' in archive_lower:
        # 尝试区分Rust代码和BoringSSL代码
        if obj_file:
            obj_lower = obj_file.lower()
            # BoringSSL object文件特征
            if any(pattern in obj_lower for pattern in [
                'ssl', 'crypto', 'asn1', 'x509', 'evp', 'rsa', 'aes',
                'sha', 'md5', 'ec_', 'bn_', 'pem', 'bio', 'des', 'chacha',
                'poly1305', 'curve25519', 'ed25519', 'hmac', 'dh_', 'dsa'
            ]):
                return 'boringssl_in_libquiche'
            # Rust代码特征
            if 'quiche-' in obj_lower or '.rcgu.o' in obj_lower:
                return 'rust_quiche'
        return 'rust_quiche'  # 默认认为是Rust代码

    # 系统库
    if 'crtbegin' in archive_lower or 'crtend' in archive_lower:
        return 'system'

    return 'unknown'

def get_rust_module_name(obj_file):
    """从Rust .o文件名提取模块名"""
    # 例如: quiche-4ab8f2f6b78ccfeb.addr2line-c9da49ecd4a3a4ea.addr2line.5e74b032b241f6c2-cgu.0.rcgu.o.rcgu.o
    # 提取: addr2line

    if not obj_file:
        return 'unknown'

    # 移除 .rcgu.o 后缀
    name = obj_file.replace('.rcgu.o', '')

    # 分割并查找模块名
    parts = name.split('.')
    if len(parts) >= 2:
        # 通常第一个带hash的是crate，第二个是模块
        module = parts[0].split('-')[0] if '-' in parts[0] else parts[0]
        if len(parts) > 1:
            submodule = parts[1].split('-')[0] if '-' in parts[1] else parts[1]
            if submodule and submodule != module:
                return f"{module}::{submodule}"
        return module

    return obj_file[:30]  # 截断长名称

def analyze_linkmap_detailed(linkmap_path):
    """详细分析linkmap，统计每个.o文件"""

    # 按archive分组的object文件统计
    archive_objects = defaultdict(lambda: defaultdict(int))

    # 按组件分类的统计
    component_stats = defaultdict(int)

    # Rust模块统计
    rust_modules = defaultdict(int)

    # Section统计
    section_stats = defaultdict(int)

    total_size = 0
    line_count = 0

    with open(linkmap_path, 'r', encoding='utf-8', errors='ignore') as f:
        # 跳过头
        header = f.readline()

        for line in f:
            line_count += 1
            parts = line.split(maxsplit=6)

            if len(parts) < 3:
                continue

            # 提取大小
            size = parse_size(parts[2])
            if size == 0:
                continue

            total_size += size

            # 提取section名称
            section = parts[4] if len(parts) >= 5 else 'unknown'
            section_stats[section] += size

            # 提取输入文件信息
            in_field = parts[5] if len(parts) >= 6 else ''
            archive, obj_file = extract_object_file(in_field)

            if archive and obj_file:
                # 统计archive中的object文件
                archive_objects[archive][obj_file] += size

                # 分类
                component = categorize_component(archive, obj_file)
                component_stats[component] += size

                # Rust模块统计
                if component in ['rust_quiche']:
                    module = get_rust_module_name(obj_file)
                    rust_modules[module] += size

    return {
        'archive_objects': archive_objects,
        'component_stats': component_stats,
        'rust_modules': rust_modules,
        'section_stats': section_stats,
        'total_size': total_size,
        'line_count': line_count
    }

def format_size(bytes_val):
    """格式化字节大小"""
    if bytes_val < 1024:
        return f"{bytes_val} B"
    elif bytes_val < 1024 * 1024:
        return f"{bytes_val / 1024:.2f} KB"
    else:
        return f"{bytes_val / (1024 * 1024):.2f} MB"

def print_detailed_report(results):
    """打印详细报告"""

    total_size = results['total_size']
    archive_objects = results['archive_objects']
    component_stats = results['component_stats']
    rust_modules = results['rust_modules']
    section_stats = results['section_stats']

    print("=" * 100)
    print("libquiche_engine.so - 详细Link Map分析 (精确到.o文件)")
    print("=" * 100)
    print()
    print(f"总大小: {format_size(total_size)}")
    print(f"处理行数: {results['line_count']:,}")
    print()

    # 1. 组件级别统计
    print("=" * 100)
    print("1. 组件级别统计")
    print("=" * 100)

    sorted_components = sorted(component_stats.items(), key=lambda x: x[1], reverse=True)

    print(f"{'组件':<30} {'大小':>15} {'占比':>10} {'说明':<30}")
    print("-" * 100)

    component_desc = {
        'rust_quiche': 'Rust QUIC协议实现',
        'boringssl_in_libquiche': 'BoringSSL (链接入libquiche.a)',
        'boringssl': 'BoringSSL (独立.a文件)',
        'cpp_engine': 'C++ Engine包装层',
        'libev': 'libev事件循环',
        'system': '系统库',
        'unknown': '未分类'
    }

    for component, size in sorted_components:
        pct = (size / total_size * 100) if total_size > 0 else 0
        desc = component_desc.get(component, '')
        print(f"{component:<30} {format_size(size):>15} {pct:>9.2f}% {desc:<30}")

    print("-" * 100)
    print(f"{'总计':<30} {format_size(total_size):>15} {'100.00%':>10}")
    print()

    # 2. Archive文件详细统计
    print("=" * 100)
    print("2. Archive文件 (.a) 详细统计")
    print("=" * 100)
    print()

    for archive in sorted(archive_objects.keys()):
        objects = archive_objects[archive]
        archive_total = sum(objects.values())

        print(f"📦 {archive} - 总计: {format_size(archive_total)} ({archive_total/total_size*100:.2f}%)")
        print("-" * 100)

        # 按大小排序object文件
        sorted_objects = sorted(objects.items(), key=lambda x: x[1], reverse=True)

        # 显示Top 30
        for obj_file, size in sorted_objects[:30]:
            pct = (size / archive_total * 100) if archive_total > 0 else 0
            print(f"  {format_size(size):>12}  ({pct:>5.2f}%)  {obj_file}")

        if len(sorted_objects) > 30:
            remaining = len(sorted_objects) - 30
            remaining_size = sum(size for _, size in sorted_objects[30:])
            print(f"  ... 还有 {remaining} 个.o文件，总计 {format_size(remaining_size)}")

        print()

    # 3. Rust模块统计
    if rust_modules:
        print("=" * 100)
        print("3. Rust模块详细统计")
        print("=" * 100)

        rust_total = sum(rust_modules.values())
        sorted_modules = sorted(rust_modules.items(), key=lambda x: x[1], reverse=True)

        print(f"Rust代码总计: {format_size(rust_total)} ({rust_total/total_size*100:.2f}%)")
        print()
        print(f"{'模块':<50} {'大小':>15} {'占比':>10}")
        print("-" * 100)

        for module, size in sorted_modules[:50]:
            pct = (size / rust_total * 100) if rust_total > 0 else 0
            print(f"{module:<50} {format_size(size):>15} {pct:>9.2f}%")

        if len(sorted_modules) > 50:
            print(f"  ... 还有 {len(sorted_modules) - 50} 个模块")
        print()

    # 4. Section统计
    print("=" * 100)
    print("4. Section统计")
    print("=" * 100)

    sorted_sections = sorted(section_stats.items(), key=lambda x: x[1], reverse=True)

    print(f"{'Section':<30} {'大小':>15} {'占比':>10}")
    print("-" * 100)

    for section, size in sorted_sections:
        pct = (size / total_size * 100) if total_size > 0 else 0
        print(f"{section:<30} {format_size(size):>15} {pct:>9.2f}%")

    print()

    # 5. BoringSSL详细分析
    print("=" * 100)
    print("5. BoringSSL详细分析")
    print("=" * 100)

    # 合并来自libquiche.a中的BoringSSL和独立的BoringSSL
    boringssl_total = component_stats.get('boringssl', 0) + component_stats.get('boringssl_in_libquiche', 0)

    print(f"BoringSSL总计: {format_size(boringssl_total)} ({boringssl_total/total_size*100:.2f}%)")
    print()

    # 收集所有BoringSSL相关的.o文件
    boringssl_objects = defaultdict(int)

    for archive, objects in archive_objects.items():
        for obj_file, size in objects.items():
            component = categorize_component(archive, obj_file)
            if component in ['boringssl', 'boringssl_in_libquiche']:
                boringssl_objects[obj_file] += size

    # 按大小排序
    sorted_bssl = sorted(boringssl_objects.items(), key=lambda x: x[1], reverse=True)

    print(f"{'Object文件':<60} {'大小':>15} {'占比':>10}")
    print("-" * 100)

    for obj_file, size in sorted_bssl[:40]:
        pct = (size / boringssl_total * 100) if boringssl_total > 0 else 0
        print(f"{obj_file:<60} {format_size(size):>15} {pct:>9.2f}%")

    if len(sorted_bssl) > 40:
        print(f"  ... 还有 {len(sorted_bssl) - 40} 个.o文件")

    print()

    # 6. 优化建议
    print("=" * 100)
    print("6. 优化建议")
    print("=" * 100)
    print()

    # 分析BoringSSL占比
    bssl_pct = (boringssl_total / total_size * 100) if total_size > 0 else 0
    print(f"BoringSSL占比: {bssl_pct:.1f}%")

    if bssl_pct > 40:
        print("  建议:")
        print("  - BoringSSL占比较高，但已通过build.rs深度裁剪")
        print("  - 查看上面的BoringSSL .o文件列表，识别最大的模块")
        print("  - 考虑是否可以禁用某些加密算法或协议")

    # 分析Rust stdlib
    rust_total = component_stats.get('rust_quiche', 0)
    rust_pct = (rust_total / total_size * 100) if total_size > 0 else 0
    print()
    print(f"Rust QUIC占比: {rust_pct:.1f}%")
    print("  - 核心功能，占比合理")

    # 查找调试相关模块
    debug_modules = {k: v for k, v in rust_modules.items() if any(
        pattern in k.lower() for pattern in ['addr2line', 'gimli', 'backtrace', 'libunwind']
    )}

    if debug_modules:
        debug_total = sum(debug_modules.values())
        print()
        print(f"调试工具占比: {format_size(debug_total)} ({debug_total/total_size*100:.1f}%)")
        print("  包含模块:")
        for module, size in sorted(debug_modules.items(), key=lambda x: x[1], reverse=True):
            print(f"    - {module}: {format_size(size)}")
        print("  建议: 生产版本可考虑禁用backtrace功能")

    print()

def main():
    if len(sys.argv) != 2:
        print("用法: python3 analyze_linkmap_detailed.py <linkmap.txt>")
        print()
        print("示例:")
        print("  python3 analyze_linkmap_detailed.py target/.../out/linkmap.txt")
        sys.exit(1)

    linkmap_path = sys.argv[1]

    print(f"分析Link Map文件: {linkmap_path}")
    print()

    try:
        results = analyze_linkmap_detailed(linkmap_path)
        print_detailed_report(results)
    except FileNotFoundError:
        print(f"错误: 找不到文件 {linkmap_path}")
        sys.exit(1)
    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
