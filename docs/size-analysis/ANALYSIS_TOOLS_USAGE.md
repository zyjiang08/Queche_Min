# 体积分析工具使用文档

本文档介绍如何使用 `analyze_symbols.py` 和 `analyze_linkmap.py` 两个工具来分析 Android 库文件的体积组成。

---

## 📋 工具概览

| 工具 | 用途 | 推荐度 | 分析维度 |
|------|------|--------|---------|
| **analyze_symbols.py** | 分析符号级别的代码大小 | ⭐⭐⭐ | 基于符号名称分类 |
| **analyze_linkmap.py** | 分析链接图文件 | ⭐⭐ | 基于源文件路径分类 |

**推荐使用**: `analyze_symbols.py` - 更准确、更易理解

---

## 🔧 工具1: analyze_symbols.py

### 功能说明

通过解析 `llvm-nm` 输出的符号信息，分析 `.so` 文件中各个组件的代码大小占比。

**优势**:
- ✅ 直接分析最终二进制文件
- ✅ 基于符号名称智能分类
- ✅ 可以分析 stripped 或 unstripped 版本
- ✅ 结果直观易懂

**原理**:
1. 使用 `llvm-nm -S --size-sort` 获取所有符号及其大小
2. 根据符号命名规则分类到不同组件:
   - BoringSSL: `RSA_`, `SSL_`, `EVP_`, `_ZN4bssl` 等
   - Rust QUIC: `_ZN6quiche` 等
   - libev: `ev_`, `_ev_` 等
   - C++ Engine: `QuicheEngine`, `quiche_engine` 等
3. 统计每个组件的总大小和占比

---

### 使用方法

#### 基本语法

```bash
python3 analyze_symbols.py <so文件路径> <llvm-nm路径>
```

#### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `<so文件路径>` | 要分析的 `.so` 文件 (绝对或相对路径) | `target/aarch64-linux-android/release/libquiche_engine.so` |
| `<llvm-nm路径>` | NDK中 `llvm-nm` 工具的完整路径 | `/path/to/ndk/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm` |

---

### 完整使用示例

#### Android ARM64 库分析

```bash
# 设置NDK路径 (根据你的NDK版本调整)
export NDK_PATH="/Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313"

# 分析 unstripped 版本 (推荐，包含完整符号信息)
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm

# 分析 stripped 版本
python3 analyze_symbols.py \
    lib/android/arm64-v8a/libquiche_engine.so \
    $NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

#### 其他Android架构

```bash
# ARM v7 (32位)
python3 analyze_symbols.py \
    target/armv7-linux-androideabi/release/libquiche_engine.so \
    $NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm

# x86_64
python3 analyze_symbols.py \
    target/x86_64-linux-android/release/libquiche_engine.so \
    $NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

---

### 输出说明

#### 1. 组件占比表

```
==========================================================================================
libquiche_engine.so - Size Composition Analysis (Based on Symbol Names)
==========================================================================================

Component                       Size   Percentage Description
------------------------------------------------------------------------------------------
boringssl                  708.90 KB       46.09% BoringSSL (SSL/TLS + crypto)
rust_stdlib                256.53 KB       16.68% Rust stdlib + debug crates
quiche_rust                111.85 KB        7.27% Rust QUIC protocol impl
...
```

**说明**:
- **Component**: 组件名称
- **Size**: 该组件的代码大小
- **Percentage**: 占总大小的百分比
- **Description**: 组件描述

#### 2. 核心组件汇总

```
==========================================================================================
CORE COMPONENTS SUMMARY
==========================================================================================
1. BoringSSL:              708.90 KB ( 46.1%)
2. Rust QUIC:              111.85 KB (  7.3%)
3. Rust Stdlib:            256.53 KB ( 16.7%)
   ----------------------------------------
   Core (1+2+3):             1.05 MB ( 70.0%)
```

**说明**: 将主要组件单独列出并计算核心功能总大小

#### 3. 组件详细符号列表

```
==========================================================================================
BORINGSSL - Top 30 Largest Symbols
==========================================================================================
        Size  Symbol
------------------------------------------------------------------------------------------
    37.62 KB  kObjects
    14.51 KB  kOpenSSLReasonStringData
     9.88 KB  _ZN4bssl20ssl_server_handshakeEPNS_13SSL_HANDSHAKEE
...
```

**说明**: 列出每个组件中最大的30个符号，帮助识别优化目标

#### 4. 优化建议

```
==========================================================================================
优化建议 / OPTIMIZATION RECOMMENDATIONS
==========================================================================================

1. BoringSSL占比: 46.1% (708.90 KB)
   ✓ 已经通过深度裁剪优化 (禁用了30+个不需要的算法和协议)
   - 进一步优化空间有限，除非禁用更多加密算法
...
```

**说明**: 根据分析结果给出具体的优化建议

---

### 组件分类规则

工具使用以下规则对符号进行分类:

#### BoringSSL
```python
# C函数前缀
RSA_, DSA_, SSL_, TLS_, BN_, EVP_, X509_, AES_, SHA_, etc.

# C++命名空间
_ZN4bssl  (namespace bssl::)

# 常量表
kObjects, kOpenSSL*, kPrimes, etc.
```

#### Rust QUIC
```python
_ZN6quiche  (namespace quiche::)
```

#### Rust Stdlib
```python
_ZN4core   (core crate)
_ZN3std    (std crate)
_ZN5alloc  (alloc crate)
_ZN9addr2line, _ZN5gimli, _ZN9libunwind  (debug crates)
```

#### libev
```python
ev_*, _ev_*, libev
```

#### C++ Engine
```python
quiche_engine, QuicheEngine, QuicheEngineImpl, CommandQueue
```

---

### 常见问题

#### Q1: stripped版本和unstripped版本分析结果有何不同？

**A**:
- **Unstripped**: 包含完整符号信息，分析更准确，**推荐使用**
- **Stripped**: 只保留动态符号，分析可能遗漏静态符号

**建议**: 使用 `target/aarch64-linux-android/release/libquiche_engine.so` (unstripped)

#### Q2: 为什么"unknown"占比较高？

**A**: "unknown"包含:
- 数据段 (常量表、静态数据)
- 未能识别的符号命名模式
- 系统库符号
- GOT/PLT表项

这是正常现象，不影响主要组件的分析。

#### Q3: 如何找到NDK中的llvm-nm？

**A**:
```bash
# 查找NDK路径
ls -d ~/Library/Android/sdk/ndk/*

# llvm-nm位置 (根据你的系统调整)
# macOS: darwin-x86_64
# Linux: linux-x86_64
<NDK路径>/toolchains/llvm/prebuilt/<系统>/bin/llvm-nm
```

#### Q4: 分析失败怎么办？

**A**: 检查以下几点:
1. `.so` 文件路径是否正确
2. `llvm-nm` 路径是否正确
3. 是否有文件读取权限
4. Python 3 是否已安装

---

## 🔧 工具2: analyze_linkmap.py

### 功能说明

通过解析链接器生成的 link map 文件，分析库文件的体积组成。

**优势**:
- ✅ 可以看到源文件级别的详细信息
- ✅ 了解链接过程中的symbol合并

**局限性**:
- ⚠️ 需要重新编译生成link map
- ⚠️ 由于使用`--whole-archive`，libquiche.a包含了BoringSSL，难以精确分离
- ⚠️ 分析结果不如 `analyze_symbols.py` 直观

**结论**: 作为备用工具，主要用于调试链接问题

---

### 使用方法

#### 基本语法

```bash
python3 analyze_linkmap.py <linkmap.txt文件路径>
```

#### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `<linkmap.txt>` | 链接器生成的map文件路径 | `target/.../out/linkmap.txt` |

---

### 生成 Link Map

Link map 由构建脚本自动生成（已添加到 `quiche/src/build.rs`）:

```bash
# 构建Android库 (会自动生成linkmap.txt)
export ANDROID_NDK_HOME=/path/to/ndk/23.2.8568313
./quiche_engine_all.sh android arm64-v8a

# linkmap.txt位置
# target/aarch64-linux-android/release/build/quiche-<hash>/out/linkmap.txt
```

#### 手动查找linkmap位置

```bash
# 方法1: 构建时查看输出
./quiche_engine_all.sh android arm64-v8a 2>&1 | grep "Link map"

# 输出示例:
# warning: quiche@0.24.6: Link map: /path/to/linkmap.txt

# 方法2: 查找文件
find target/aarch64-linux-android/release/build -name "linkmap.txt"
```

---

### 使用示例

```bash
# 分析linkmap
python3 analyze_linkmap.py \
    target/aarch64-linux-android/release/build/quiche-9604f2b623922bbf/out/linkmap.txt
```

---

### 输出说明

```
================================================================================
libquiche_engine.so - Size Composition Analysis
================================================================================

Component                       Size   Percentage
--------------------------------------------------------------------------------
unknown                     24.88 MB      100.00%
--------------------------------------------------------------------------------
TOTAL                       24.88 MB      100.00%
```

**注意**: 由于当前构建配置使用 `--whole-archive` 链接 `libquiche.a`（其中已包含BoringSSL），linkmap中所有符号都显示为来自 `libquiche.a`，因此无法精确分离各组件。

这就是为什么推荐使用 `analyze_symbols.py` 的原因。

---

### 局限性说明

#### 当前问题

由于构建配置使用:
```bash
-Wl,--whole-archive libquiche.a -Wl,--no-whole-archive
```

`libquiche.a` 包含了:
- Rust QUIC代码
- BoringSSL (通过静态链接合并)

因此在linkmap中，所有这些符号都显示为来自 `libquiche.a`，无法区分。

#### 解决方案

如果需要使用linkmap进行精确分析，需要修改构建流程:
1. 分离 BoringSSL 构建
2. 不使用 `--whole-archive`
3. 单独链接各个组件

**但这会增加构建复杂度，不推荐。**

---

## 📊 两个工具的对比

| 特性 | analyze_symbols.py | analyze_linkmap.py |
|------|-------------------|-------------------|
| **分析维度** | 符号名称 | 源文件路径 |
| **准确性** | ⭐⭐⭐ 高 | ⭐⭐ 中 (受链接方式影响) |
| **易用性** | ⭐⭐⭐ 简单 | ⭐⭐ 中等 |
| **速度** | ⭐⭐⭐ 快 | ⭐⭐ 较快 |
| **详细程度** | ⭐⭐⭐ 符号级别 | ⭐⭐ 文件级别 |
| **是否需要重新编译** | ❌ 否 | ✅ 是 (需生成linkmap) |
| **stripped版本支持** | ⚠️ 部分支持 | ✅ 支持 |
| **推荐使用场景** | 常规体积分析 | 调试链接问题 |

---

## 💡 最佳实践

### 推荐工作流

1. **日常分析**: 使用 `analyze_symbols.py`
   ```bash
   python3 analyze_symbols.py \
       target/aarch64-linux-android/release/libquiche_engine.so \
       $NDK_PATH/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
   ```

2. **链接问题调试**: 使用 `analyze_linkmap.py`
   - 当需要了解链接器如何组织section时
   - 当需要检查是否有重复符号时
   - 当需要验证链接脚本是否正确时

3. **对比分析**: 结合使用两个工具
   - `analyze_symbols.py` 看组件占比
   - `analyze_linkmap.py` 看section布局

---

### 优化工作流示例

```bash
# 1. 基线分析
python3 analyze_symbols.py lib/baseline/libquiche_engine.so $NDK_NM > baseline.txt

# 2. 实施优化 (例如: 禁用某个BoringSSL功能)
# ... 修改配置并重新编译 ...

# 3. 对比分析
python3 analyze_symbols.py lib/optimized/libquiche_engine.so $NDK_NM > optimized.txt

# 4. 对比结果
diff baseline.txt optimized.txt
```

---

## 🔍 高级用法

### 自定义分类规则

如果你需要添加新的组件分类，可以修改 `analyze_symbols.py` 中的 `categorize_symbol` 函数:

```python
def categorize_symbol(symbol_name):
    # 添加你的自定义规则
    if 'my_custom_lib' in symbol_name:
        return 'my_component'

    # 原有规则...
```

### 导出为CSV格式

修改脚本输出，便于Excel分析:

```bash
# 运行脚本并提取关键数据
python3 analyze_symbols.py ... | grep -A 100 "Component.*Size.*Percentage" > results.csv
```

### 批量分析多个架构

```bash
#!/bin/bash
# analyze_all_archs.sh

NDK_NM="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm"

for arch in arm64-v8a armeabi-v7a x86_64 x86; do
    echo "Analyzing $arch..."
    python3 analyze_symbols.py \
        lib/android/$arch/libquiche_engine.so \
        $NDK_NM \
        > analysis_$arch.txt
done
```

---

## 📝 输出保存

### 保存完整报告

```bash
# 保存到文件
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $NDK_NM \
    > SIZE_ANALYSIS_$(date +%Y%m%d_%H%M%S).txt

# 同时显示并保存
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $NDK_NM \
    | tee SIZE_ANALYSIS_$(date +%Y%m%d_%H%M%S).txt
```

---

## 🐛 故障排除

### 错误: "No such file or directory: llvm-nm"

**原因**: NDK路径不正确

**解决**:
```bash
# 查找NDK安装位置
find ~/Library/Android/sdk -name "llvm-nm" 2>/dev/null

# 或者
locate llvm-nm | grep android
```

### 错误: "Permission denied"

**原因**: 没有执行权限

**解决**:
```bash
chmod +x analyze_symbols.py
chmod +x analyze_linkmap.py
```

### 输出为空或异常

**原因**: 可能是stripped版本，符号被删除

**解决**: 使用unstripped版本
```bash
# Unstripped版本位置
target/<架构>/release/libquiche_engine.so

# 而不是
lib/android/<架构>/libquiche_engine.so
```

---

## 📚 相关文档

- **SIZE_ANALYSIS_REPORT.md** - 完整的体积分析报告
- **OPTIMIZATION_SUMMARY.md** - 优化总结文档
- **Cargo.toml** - Rust编译配置
- **quiche/src/build.rs** - 构建脚本 (生成linkmap)

---

## 🎓 深入理解

### 符号命名规范 (Name Mangling)

#### Rust符号
```
_ZN6quiche10Connection11recv_single17h3379428d156365c5E
 │  │      └────┬────┘ └───┬───┘ └──────┬─────┘
 │  │          函数路径      函数名      哈希值
 │  └─ 命名空间层级数
 └─ Rust mangling前缀
```

#### C++符号
```
_ZN4bssl20ssl_server_handshakeEPNS_13SSL_HANDSHAKEE
 │  │   │  └───────┬───────┘  └────┬────────────┘
 │  │   │         函数名           参数类型
 │  │   └─ 字符数
 │  └─ 命名空间
 └─ C++ mangling前缀
```

#### C符号
```
RSA_generate_key_ex
└──────┬──────────┘
    原始函数名 (未修饰)
```

### Link Map 文件格式

```
VMA              LMA     Size Align Out     In      Symbol
38c80            38c80   2    1     .rodata libquiche.a(...):(.rodata.anon...)
│                │       │    │     │       │               └─ Section
│                │       │    │     │       └─ 源文件
│                │       │    │     └─ 输出section
│                │       │    └─ 对齐
│                │       └─ 大小 (hex)
│                └─ Load Memory Address
└─ Virtual Memory Address
```

---

## ✅ 快速参考

### analyze_symbols.py

```bash
# 最常用命令
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    /path/to/ndk/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

### analyze_linkmap.py

```bash
# 查找linkmap
find target -name "linkmap.txt"

# 分析
python3 analyze_linkmap.py target/.../out/linkmap.txt
```

---

**文档版本**: 1.0
**最后更新**: 2025-11-08
**适用平台**: Android (ARM64, ARM, x86_64, x86)
