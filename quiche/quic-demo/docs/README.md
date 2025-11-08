# QUIC Demo Documentation

本目录包含 QUIC Demo 相关的所有技术文档，按类别组织。

## 📁 文档目录

### 🤖 Android 相关文档
位置: `android/`

- **[README_ANDROID.md](android/README_ANDROID.md)** - Android平台使用指南
- **[ANDROID_BUILD_STATUS.md](android/ANDROID_BUILD_STATUS.md)** - Android构建状态报告
- **[ANDROID_LINKING_FIX.md](android/ANDROID_LINKING_FIX.md)** - Android链接问题修复
- **[ANDROID_SYMBOL_ANALYSIS.md](android/ANDROID_SYMBOL_ANALYSIS.md)** - Android符号分析

### ⚡ 优化相关文档
位置: `optimization/`

- **[DEEP_OPTIMIZATION_PLAN.md](optimization/DEEP_OPTIMIZATION_PLAN.md)** - 深度优化计划
- **[OPTIMIZATION_SUMMARY.md](optimization/OPTIMIZATION_SUMMARY.md)** - 优化总结报告
- **[OPTIMIZATION_FINAL.md](optimization/OPTIMIZATION_FINAL.md)** - 最终优化报告
- **[OPTIMIZATION_RESULTS_FINAL.md](optimization/OPTIMIZATION_RESULTS_FINAL.md)** - 最终优化结果
- **[OPTIMIZATION_QUICK_START_V2.md](optimization/OPTIMIZATION_QUICK_START_V2.md)** - 优化快速开始指南 V2
- **[OPTIMIZATION_QUICK_START.md](optimization/OPTIMIZATION_QUICK_START.md)** - 优化快速开始指南
- **[OPTIMIZATION_RESULTS.md](optimization/OPTIMIZATION_RESULTS.md)** - 优化结果
- **[OPTIMIZATION.md](optimization/OPTIMIZATION.md)** - 优化文档

### 🔧 开发相关文档
位置: `development/`

- **[CODE_UNIFICATION.md](development/CODE_UNIFICATION.md)** - 代码统一说明

## 📖 主要文档

根目录下的主要文档:
- **[../README.md](../README.md)** - QUIC Demo 主要说明文档

## 🚀 快速导航

### 按用途查找

**Android开发者**:
1. 从 [README_ANDROID.md](android/README_ANDROID.md) 开始
2. 如遇构建问题，查看 [ANDROID_BUILD_STATUS.md](android/ANDROID_BUILD_STATUS.md)
3. 如遇链接错误，查看 [ANDROID_LINKING_FIX.md](android/ANDROID_LINKING_FIX.md)

**性能优化**:
1. 从 [DEEP_OPTIMIZATION_PLAN.md](optimization/DEEP_OPTIMIZATION_PLAN.md) 了解优化策略
2. 查看 [OPTIMIZATION_SUMMARY.md](optimization/OPTIMIZATION_SUMMARY.md) 了解优化成果
3. 使用 [OPTIMIZATION_QUICK_START_V2.md](optimization/OPTIMIZATION_QUICK_START_V2.md) 快速应用优化

**代码维护**:
1. 查看 [CODE_UNIFICATION.md](development/CODE_UNIFICATION.md) 了解代码统一情况

## 📝 文档更新日志

- **2025-11-08**: 文档重组，按类别分类到 android/, optimization/, development/ 子目录
- **2025-11-08**: Android相关文档完善（构建状态、链接修复、符号分析）
- **2025-11-08**: 优化文档系列完善（深度优化计划、优化总结）
- **2025-11-08**: 代码统一文档添加

## 🔗 相关资源

### 构建脚本
- `../Makefile` - 主要构建配置（macOS）
- `../Makefile.android` - Android交叉编译配置

### 示例程序
- `../src/client.cpp` - QUIC客户端示例
- `../src/server.cpp` - QUIC服务器示例

### 证书文件
- `../ca.crt` - CA证书
- `../server.crt` / `../server.key` - 服务器证书

## 💡 贡献指南

添加新文档时:
1. 根据文档类型放到对应目录:
   - Android相关 → `android/`
   - 性能优化 → `optimization/`
   - 代码开发 → `development/`
2. 更新本 README.md，添加文档链接
3. 在文档开头说明文档用途和适用场景

## 📞 获取帮助

- 查看主 [README.md](../README.md) 了解基本用法
- 查看对应类别的文档索引
- 查看 `../../docs/` 目录了解项目级文档

---

**最后更新**: 2025-11-08
