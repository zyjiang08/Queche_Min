# libquiche_engine 体积分析文档

本目录包含libquiche_engine库的完整体积分析文档和工具。

---

## 📊 核心数据概览

| 指标 | 数值 |
|------|------|
| **最终库大小 (Stripped)** | **2.1 MB** ✅ |
| Unstripped大小 (含调试) | 24.88 MB |
| Android版本 (Stripped) | 1.4 MB |
| 优化程度 | 深度优化完成 |

### 组件组成 (基于符号分析)

| 组件 | 大小 | 占比 |
|------|------|------|
| BoringSSL (SSL/TLS + 加密) | 708.90 KB | 46.1% |
| Rust QUIC协议实现 | 111.85 KB | 7.3% |
| Rust标准库 + 调试工具 | 256.53 KB | 16.7% |
| C++ Engine包装层 | 45.23 KB | 2.9% |
| libev事件循环 | 23.12 KB | 1.5% |
| 其他 | ~392 KB | ~25.5% |

---

## 📚 文档索引

### 主要报告

| 文档 | 说明 | 优先级 |
|------|------|--------|
| **[LINKMAP_ANALYSIS_COMPLETE.md](LINKMAP_ANALYSIS_COMPLETE.md)** | 🌟 **完整Link Map分析报告** (最新、最全面) | ⭐⭐⭐ |
| [SIZE_ANALYSIS_REPORT.md](SIZE_ANALYSIS_REPORT.md) | 体积分析详细报告 | ⭐⭐ |
| [OPTIMIZATION_SUMMARY.md](OPTIMIZATION_SUMMARY.md) | 优化措施总结 | ⭐⭐ |

### 工具使用文档

| 文档 | 说明 |
|------|------|
| [ANALYSIS_TOOLS_README.md](ANALYSIS_TOOLS_README.md) | 分析工具快速入门 (推荐先读) |
| [ANALYSIS_TOOLS_USAGE.md](ANALYSIS_TOOLS_USAGE.md) | 分析工具详细使用指南 |

---

## 🛠️ 分析工具

所有工具位于 [`tools/`](tools/) 目录：

| 工具 | 用途 | 推荐度 |
|------|------|--------|
| **[analyze_symbols.py](tools/analyze_symbols.py)** | ⭐ 符号级别分析 (推荐日常使用) | ⭐⭐⭐ |
| [analyze_linkmap_detailed.py](tools/analyze_linkmap_detailed.py) | Link Map详细分析 (精确到.o文件) | ⭐⭐ |
| [analyze_linkmap.py](tools/analyze_linkmap.py) | Link Map基础分析 (备用) | ⭐ |

### 快速开始

#### 1. 符号分析 (推荐)

```bash
python3 tools/analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

**输出内容**:
- 各组件大小占比
- Top 30最大符号 (每个组件)
- 优化建议

#### 2. Link Map分析

```bash
python3 tools/analyze_linkmap_detailed.py \
    target/aarch64-linux-android/release/build/quiche-xxx/out/linkmap.txt
```

**输出内容**:
- Section级别分解 (.text, .rodata, .debug_*)
- Archive文件 (.a) 详细统计
- .o文件贡献分析
- Rust模块统计

---

## 🎯 关键发现

### 1. 体积组成

基于符号分析的精确数据:
- **BoringSSL占46.1%**: SSL/TLS功能 + 加密算法，已深度优化
- **Rust QUIC占7.3%**: QUIC协议核心，占比合理
- **Rust Stdlib占16.7%**: 包含调试工具(addr2line, gimli)，有优化空间

### 2. 优化成果

| 优化措施 | 效果 |
|----------|------|
| BoringSSL裁剪 (30+ features) | 预计-30-40% |
| Rust编译优化 (opt-level=z, LTO) | 预计-15-20% |
| 链接器优化 (--gc-sections) | 预计-5-10% |
| Strip调试符号 | -83% (24.88 MB → 2.1 MB) |

### 3. Link Map vs 符号分析

| 方面 | Link Map | 符号分析 |
|------|----------|----------|
| 精度 | Section级别 | 符号级别 |
| 组件分类 | 受限于链接方式 | ✅ 准确 |
| .o文件可见性 | 部分可见 | 不可见 |
| 调试信息 | ✅ 可见 | 不可见 |
| 最终大小反映 | 不准确(含调试) | ✅ 准确 |
| 推荐用途 | 调试链接问题 | 日常体积检查 |

**结论**: 两种方法互补，符号分析更适合日常使用。

---

## 💡 优化建议

### 当前状态: ✅ 生产就绪

**2.1 MB大小已经非常优秀**，无需进一步优化。

### 进一步优化选项 (可选)

| 选项 | 预期效果 | 收益 | 风险 |
|------|----------|------|------|
| **禁用backtrace** | 1.9-2.0 MB | -6%~-10% | 低 (仅影响crash报告) |
| BoringSSL微调 | 2.0-2.05 MB | -3%~-5% | 中 (需测试兼容性) |
| 禁用HTTP/3 | 1.3-1.5 MB | -30%~-40% | 高 (失去HTTP/3功能) |

### 推荐方案

**保持当前版本 (2.1 MB)**

理由:
- ✅ 已深度优化
- ✅ 功能完整 (QUIC + HTTP/3)
- ✅ 体积合理
- ✅ 可维护性好
- ✅ 调试友好

**如需更小**: 仅禁用backtrace → 1.9-2.0 MB

---

## 📖 详细内容

### LINKMAP_ANALYSIS_COMPLETE.md (完整分析报告)

**包含内容**:
1. Link Map分析 - Section级别分解
2. 符号级别分析 - 组件组成
3. Object文件(.o)级别分析 - 尽可能详细
4. 体积优化总结 - 所有优化措施
5. 进一步优化潜力 - 4种优化选项
6. 技术限制与说明 - 分析方法对比
7. 结论与建议 - 最终建议

**重点章节**:
- 第二部分: 符号级别分析 (最准确的组件分解)
- 第五部分: 进一步优化潜力 (优化选项)
- 第七部分: 结论与建议 (最终建议)

### SIZE_ANALYSIS_REPORT.md

早期的体积分析报告，包含基础的符号分析结果。

### OPTIMIZATION_SUMMARY.md

总结了所有已实施的优化措施:
- Rust编译优化配置
- BoringSSL深度裁剪
- 链接器优化
- Strip优化

---

## 🔍 常见问题

### Q: 如何快速了解库的体积组成？

**A**: 运行符号分析工具:
```bash
python3 tools/analyze_symbols.py <库文件> <llvm-nm路径>
```

### Q: 为什么Link Map显示24.88 MB，而最终库只有2.1 MB？

**A**: Link Map包含调试信息(~91.6%):
- .debug_info, .debug_loc, .debug_line等section
- Strip后移除这些section，从24.88 MB降至2.1 MB

### Q: 为什么Link Map无法精确分离.o文件？

**A**: 构建使用`--whole-archive`链接libquiche.a(已包含BoringSSL)，链接器将其视为单一输入。解决方案：使用符号分析代替。

### Q: BoringSSL占46%，能否进一步减少？

**A**: 已经通过禁用30+个features深度优化，进一步优化空间有限(<5%)，且可能影响兼容性。

### Q: Rust Stdlib占16.7%，能否减少？

**A**: 可以通过禁用backtrace功能减少6-10%，但会影响调试体验。生产环境可考虑。

### Q: 最小能做到多小？

**A**:
- 当前: 2.1 MB (全功能)
- 禁用backtrace: 1.9-2.0 MB
- 禁用HTTP/3: 1.3-1.5 MB (仅QUIC传输)

---

## 🚀 使用场景

### 场景1: 日常体积检查

```bash
# 快速查看当前库的体积组成
python3 tools/analyze_symbols.py \
    target/aarch64-linux-android/release/libquiche_engine.so \
    $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm
```

### 场景2: 优化前后对比

```bash
# 保存基线
python3 tools/analyze_symbols.py ... > baseline.txt

# 实施优化后
python3 tools/analyze_symbols.py ... > optimized.txt

# 对比
diff baseline.txt optimized.txt
```

### 场景3: 识别大符号

查看输出中的 "Top 30 Largest Symbols" 部分，找到最大的函数/数据，评估是否可优化。

### 场景4: 调试链接问题

```bash
# 生成Link Map并分析
python3 tools/analyze_linkmap_detailed.py \
    target/.../out/linkmap.txt
```

---

## 📞 技术支持

### 文档路径

本目录: `/Users/jiangzhongyang/work/live/CDN/study/Queche_Min/docs/size-analysis/`

### 相关文档

- [Android项目完成总结](../../ANDROID_PROJECT_COMPLETE.md)
- [代码统一状态](../../CODE_UNIFIED_FINAL_STATUS.md)
- [验证过程记录](../../VERIFICATION_SUMMARY.md)

### 工具要求

- Python 3.x
- llvm-nm (Android NDK)
- Link Map文件 (可选，由build.rs生成)

---

## 📝 更新历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2025-11-08 | 1.0 | 创建完整文档体系 |

---

**最后更新**: 2025-11-08
**维护者**: QUIC Engine Team
**项目状态**: ✅ 生产就绪，深度优化完成
