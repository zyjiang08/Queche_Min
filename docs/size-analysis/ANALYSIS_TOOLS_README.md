# 体积分析工具 - 快速入门

本目录包含两个用于分析 Android 库文件体积的工具。

---

## 🚀 快速开始

### 推荐工具: analyze_symbols.py ⭐

**一键分析**:
```bash
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    /Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

**输出内容**:
- 各组件大小占比 (BoringSSL, Rust QUIC, Rust Stdlib等)
- 最大符号列表 (Top 30)
- 优化建议

---

## 📁 文件说明

| 文件 | 用途 |
|------|------|
| **analyze_symbols.py** | ⭐ 符号级别分析 (推荐) |
| **analyze_linkmap.py** | 链接图分析 (备用) |
| **ANALYSIS_TOOLS_USAGE.md** | 📖 详细使用文档 |
| **SIZE_ANALYSIS_REPORT.md** | 📊 完整分析报告 |

---

## 💡 使用场景

### 场景1: 日常体积检查
```bash
# 快速查看当前库的体积组成
python3 analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

### 场景2: 优化前后对比
```bash
# 保存基线
python3 analyze_symbols.py ... > baseline.txt

# 实施优化后
python3 analyze_symbols.py ... > optimized.txt

# 对比
diff baseline.txt optimized.txt
```

### 场景3: 识别大符号
查看输出中的 "Top 30 Largest Symbols" 部分，找到最大的函数/数据，评估是否可优化。

---

## 📊 典型输出示例

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

---

## 🔍 常见问题

### Q: 如何找到NDK中的llvm-nm？

**A**:
```bash
# 查找NDK安装位置
ls -d ~/Library/Android/sdk/ndk/*

# llvm-nm完整路径
<NDK路径>/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

### Q: stripped版本可以分析吗？

**A**: 可以，但推荐使用unstripped版本以获得更准确的结果。

**Unstripped位置**: `target/aarch64-linux-android/release/libquiche_engine.so`
**Stripped位置**: `lib/android/arm64-v8a/libquiche_engine.so`

### Q: analyze_linkmap.py 什么时候用？

**A**: 仅在调试链接问题时使用。日常分析推荐使用 `analyze_symbols.py`。

---

## 📖 完整文档

详细使用说明请查看: **[ANALYSIS_TOOLS_USAGE.md](ANALYSIS_TOOLS_USAGE.md)**

包含:
- 详细参数说明
- 多平台使用示例
- 高级用法
- 故障排除
- 符号命名规范
- Link Map格式说明

---

## 🎯 核心结论 (基于已完成的分析)

### 当前库大小: 2.1M (stripped)

**组成**:
- BoringSSL: 46.1%
- Rust QUIC: 7.3%
- Rust Stdlib: 16.7%

**状态**: ✅ 已深度优化，生产就绪

**进一步优化潜力**:
- 禁用Backtrace: 可再减少 6-10% → **1.9-2.0M**
- BoringSSL进一步裁剪: 收益有限 (< 5%)

---

## 📞 需要帮助？

1. 查看 **ANALYSIS_TOOLS_USAGE.md** 获取详细说明
2. 查看 **SIZE_ANALYSIS_REPORT.md** 了解完整分析结果
3. 检查工具输出中的 "优化建议" 部分

---

**最后更新**: 2025-11-08
