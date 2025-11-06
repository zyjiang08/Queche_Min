# build_mobile_libs.sh 使用指南

## 📖 概述

`build_mobile_libs.sh` 是一个自动化脚本，用于构建 iOS、macOS 和 Android 平台的 libquiche_engine 库。

## 🚀 基本用法

### 语法

```bash
./build_mobile_libs.sh [选项1] [选项2] ...
```

### 支持的选项

| 选项 | 描述 | 构建时间 |
|------|------|----------|
| `ios` | 构建 iOS arm64 库（真机） | ~2 分钟 |
| `ios:arm64` | 构建 iOS arm64（真机，显式） | ~2 分钟 |
| `ios:x86_64` | 构建 iOS x86_64（模拟器） | ~2 分钟 |
| `macos` | 构建 macOS 库（自动检测架构） | ~2 分钟 |
| `macos:arm64` | 构建 macOS arm64（Apple Silicon） | ~2 分钟 |
| `macos:x86_64` | 构建 macOS x86_64（Intel） | ~2 分钟 |
| `android` | 构建 Android 所有架构 | ~8 分钟 |
| `android:arm64-v8a` | 只构建 Android arm64-v8a | ~2 分钟 |
| `android:armeabi-v7a` | 只构建 Android armeabi-v7a | ~2 分钟 |
| `android:x86` | 只构建 Android x86 | ~2 分钟 |
| `android:x86_64` | 只构建 Android x86_64 | ~2 分钟 |
| `all` | 构建所有平台和架构 | ~12 分钟 |

## 📋 使用场景

### 场景 1: 开发 iOS 应用（真机）

```bash
# 构建 iOS 真机库
./build_mobile_libs.sh ios
# 或显式指定
./build_mobile_libs.sh ios:arm64
```

**输出**:
```
mobile_libs/ios/arm64/libquiche_engine.a
mobile_libs/ios/include/quiche_engine.h
```

### 场景 2: iOS 模拟器开发

在 Mac 上使用 iOS 模拟器进行开发：

```bash
# 构建 iOS 模拟器库
./build_mobile_libs.sh ios:x86_64
```

**输出**:
```
mobile_libs/ios/x86_64/libquiche_engine.a
mobile_libs/ios/include/quiche_engine.h
```

**提示**: 模拟器和真机使用不同的架构，需要分别构建。

### 场景 3: 开发 macOS 应用

在 Mac 上开发原生 macOS 应用：

```bash
# 自动检测架构（推荐）
./build_mobile_libs.sh macos

# 或显式指定架构
./build_mobile_libs.sh macos:arm64     # Apple Silicon (M1/M2/M3)
./build_mobile_libs.sh macos:x86_64    # Intel Mac
```

**输出**:
```
mobile_libs/macos/arm64/libquiche_engine.a       # 或 x86_64/
mobile_libs/macos/include/quiche_engine.h
```

**使用场景**:
- macOS 桌面应用开发
- 命令行工具开发
- 跨平台应用的 macOS 版本

**提示**:
- Apple Silicon Mac 默认构建 arm64
- Intel Mac 默认构建 x86_64
- 可通过 Rosetta 2 运行对方架构

### 场景 4: 开发 Android 应用（单架构）

推荐在开发阶段只构建需要的架构，大幅缩短构建时间：

```bash
# 大多数现代 Android 设备使用 arm64-v8a
./build_mobile_libs.sh android:arm64-v8a
```

**输出**:
```
mobile_libs/android/arm64-v8a/libquiche_engine.so
mobile_libs/android/include/quiche_engine.h
```

**构建时间对比**:
- 所有架构: ~8 分钟
- 单个架构: ~2 分钟 ⚡ **节省 75% 时间**

### 场景 5: 发布 Android 应用

发布前构建所有架构，确保最大兼容性：

```bash
# 构建所有 Android 架构
./build_mobile_libs.sh android
```

**输出**:
```
mobile_libs/android/arm64-v8a/libquiche_engine.so
mobile_libs/android/armeabi-v7a/libquiche_engine.so
mobile_libs/android/x86/libquiche_engine.so
mobile_libs/android/x86_64/libquiche_engine.so
mobile_libs/android/include/quiche_engine.h
```

### 场景 6: 跨平台开发（iOS、macOS、Android）

同时开发多个平台的应用，快速迭代：

```bash
# iOS 真机 + macOS + Android 真机
./build_mobile_libs.sh ios:arm64 macos android:arm64-v8a

# 或者只构建 iOS + macOS（Apple 生态）
./build_mobile_libs.sh ios macos
```

**输出**:
```
mobile_libs/ios/arm64/libquiche_engine.a
mobile_libs/ios/include/quiche_engine.h
mobile_libs/macos/arm64/libquiche_engine.a
mobile_libs/macos/include/quiche_engine.h
mobile_libs/android/arm64-v8a/libquiche_engine.so
mobile_libs/android/include/quiche_engine.h
```

### 场景 7: CI/CD 发布构建

```bash
# 构建所有平台和架构
./build_mobile_libs.sh all
```

## 🎯 架构选择指南

### iOS 架构对应设备

| 架构 | 设备类型 | 使用场景 |
|------|----------|----------|
| **arm64** | iPhone 5s+ / iPad Air+ | 真机测试和发布 ⭐ |
| **x86_64** | iOS 模拟器（Mac） | 开发和调试 |

**注意**:
- 真机和模拟器使用不同的架构，需要分别构建
- 大多数开发在模拟器上进行，发布前在真机测试
- M1/M2/M3 Mac 可以直接运行 arm64 iOS 应用

### macOS 架构对应设备

| 架构 | 设备类型 | 使用场景 |
|------|----------|----------|
| **arm64** | Apple Silicon (M1/M2/M3/M4) | 现代 Mac 桌面应用 ⭐ |
| **x86_64** | Intel Mac | 传统 Intel Mac 支持 |

**注意**:
- Apple Silicon 是当前主流（2020年后的 Mac）
- Intel Mac 可通过 Rosetta 2 运行 arm64 应用（有性能损失）
- arm64 Mac 无法直接运行 x86_64 原生应用
- 使用 `macos` 选项自动检测当前架构（推荐）

### Android 架构对应设备

| 架构 | 设备类型 | 市场占有率 |
|------|----------|------------|
| **arm64-v8a** | 现代 Android 设备（2014+） | ~85% ⭐ |
| **armeabi-v7a** | 旧款 Android 设备（2010-2014） | ~10% |
| **x86_64** | Intel 处理器设备、模拟器 | ~3% |
| **x86** | 旧款 Intel 设备、模拟器 | ~2% |

### 推荐策略

#### iOS 开发
```bash
# 模拟器开发（最快）
./build_mobile_libs.sh ios:x86_64

# 真机测试
./build_mobile_libs.sh ios:arm64  # 或 ios

# 发布（只需真机）
./build_mobile_libs.sh ios
```

#### macOS 开发
```bash
# 开发（自动检测架构，最方便）
./build_mobile_libs.sh macos

# 或显式指定架构
./build_mobile_libs.sh macos:arm64    # Apple Silicon
./build_mobile_libs.sh macos:x86_64   # Intel Mac

# 发布（通常只需当前架构）
./build_mobile_libs.sh macos
```

#### Android 开发
```bash
# 开发阶段：只构建主要架构（最快）
./build_mobile_libs.sh android:arm64-v8a

# 测试阶段：构建主要架构 + 兼容性架构
./build_mobile_libs.sh android:arm64-v8a
./build_mobile_libs.sh android:armeabi-v7a

# 发布阶段：构建所有架构（最大兼容性）
./build_mobile_libs.sh android
```

## ⚡ 性能对比

### 构建时间（M1 Mac）

| 命令 | 构建内容 | 时间 | 适用场景 |
|------|----------|------|----------|
| `ios` 或 `ios:arm64` | iOS arm64（真机） | ~2 分钟 | iOS 真机开发 |
| `ios:x86_64` | iOS x86_64（模拟器） | ~2 分钟 | iOS 模拟器开发 |
| `macos` 或 `macos:arm64` | macOS arm64 | ~2 分钟 | macOS 开发 |
| `macos:x86_64` | macOS x86_64 | ~2 分钟 | Intel Mac 开发 |
| `android:arm64-v8a` | Android arm64 | ~2 分钟 | Android 开发 |
| `ios macos android:arm64-v8a` | iOS + macOS + Android | ~6 分钟 | 全平台开发 |
| `android` | Android 4 架构 | ~8 分钟 | Android 发布 |
| `all` | iOS + macOS + Android 全架构 | ~12 分钟 | 完整发布 |

### 库大小

| 平台 | 架构 | Debug | Release | Stripped |
|------|------|-------|---------|----------|
| iOS | arm64 | ~8 MB | ~3 MB | ~2 MB |
| iOS | x86_64 | ~8 MB | ~3 MB | ~2 MB |
| macOS | arm64 | ~8 MB | ~3 MB | ~2 MB |
| macOS | x86_64 | ~9 MB | ~3.5 MB | ~2.3 MB |
| Android | arm64-v8a | ~6 MB | ~2 MB | ~1.5 MB |
| Android | armeabi-v7a | ~5 MB | ~1.8 MB | ~1.3 MB |
| Android | x86_64 | ~7 MB | ~2.2 MB | ~1.6 MB |

## 🔧 高级用法

### 组合多个选项

脚本支持同时指定多个选项：

```bash
# ✅ 正确：构建 iOS + macOS + Android
./build_mobile_libs.sh ios macos android:arm64-v8a

# ✅ 正确：构建 Apple 生态（iOS + macOS）
./build_mobile_libs.sh ios macos

# ✅ 正确：构建特定架构组合
./build_mobile_libs.sh ios:x86_64 macos:arm64

# ✅ 正确：构建多个 Android 架构
# 注意：需要分别执行
./build_mobile_libs.sh android:arm64-v8a
./build_mobile_libs.sh android:armeabi-v7a

# ❌ 错误：不能在一个命令中指定多个同平台架构
./build_mobile_libs.sh android:arm64-v8a android:armeabi-v7a  # 不支持
./build_mobile_libs.sh ios:arm64 ios:x86_64  # 不支持
```

### 查看帮助信息

```bash
./build_mobile_libs.sh
```

输出：
```
Usage: ./build_mobile_libs.sh [ios[:arch]] [macos[:arch]] [android[:arch]] [all]

Options:
  ios                    - Build for iOS arm64 (device)
  ios:arm64              - Build for iOS arm64 (device) explicitly
  ios:x86_64             - Build for iOS simulator (x86_64)
  macos                  - Build for macOS (current architecture)
  macos:arm64            - Build for macOS Apple Silicon (M1/M2/M3)
  macos:x86_64           - Build for macOS Intel
  android                - Build for Android (all architectures)
  android:arm64-v8a      - Build for Android arm64-v8a only
  android:armeabi-v7a    - Build for Android armeabi-v7a only
  android:x86            - Build for Android x86 only
  android:x86_64         - Build for Android x86_64 only
  all                    - Build for iOS, macOS, and Android (all architectures)

Examples:
  ./build_mobile_libs.sh ios                    # Build iOS only
  ./build_mobile_libs.sh macos                  # Build macOS (auto-detect arch)
  ./build_mobile_libs.sh android                # Build all Android architectures
  ./build_mobile_libs.sh android:arm64-v8a      # Build Android arm64-v8a only
  ./build_mobile_libs.sh ios macos android:arm64-v8a  # Build all platforms
```

## 🐛 常见问题

### Q: 如何加快构建速度？

**A**: 开发时只构建需要的架构：
```bash
# 快 75%
./build_mobile_libs.sh android:arm64-v8a

# 而不是
./build_mobile_libs.sh android
```

### Q: 应该构建哪些架构？

**A**:
- **开发**: `arm64-v8a`（覆盖 85% 设备）
- **发布**: 所有架构（最大兼容性）

### Q: 可以只构建两个架构吗？

**A**: 可以，分别运行两次：
```bash
./build_mobile_libs.sh android:arm64-v8a
./build_mobile_libs.sh android:armeabi-v7a
```

### Q: 构建失败怎么办？

**A**: 检查前置要求：
```bash
# Android
echo $ANDROID_NDK_HOME
rustup target list | grep android

# iOS
rustup target list | grep ios
xcode-select --print-path

# macOS
rustup target list | grep darwin
```

### Q: macOS 应该构建哪个架构？

**A**:
- **Apple Silicon (M1/M2/M3)**: 使用 `macos` 或 `macos:arm64`
- **Intel Mac**: 使用 `macos:x86_64`
- **不确定**: 直接使用 `macos`，会自动检测
- **验证当前架构**: 运行 `uname -m`（arm64 或 x86_64）

## 📊 实战示例

### 示例 1: 快速原型开发

目标：最快速度验证 Android 功能

```bash
# 只构建 arm64（大多数现代设备）
./build_mobile_libs.sh android:arm64-v8a

# 2 分钟后可以开始测试
```

### 示例 2: macOS 桌面应用开发

目标：快速开发 macOS 原生应用

```bash
# 自动检测架构构建（推荐）
./build_mobile_libs.sh macos

# 2 分钟后可以开始 macOS 应用开发
```

### 示例 3: 跨平台开发（Apple 生态）

目标：同时开发 iOS、macOS 和 Android 应用

```bash
# 构建三个平台的主要架构
./build_mobile_libs.sh ios macos android:arm64-v8a

# 6 分钟后三个平台都可以测试
```

### 示例 4: 发布准备

目标：准备提交到应用商店

```bash
# iOS 发布
./build_mobile_libs.sh ios

# macOS 发布
./build_mobile_libs.sh macos

# Android 发布（所有架构）
./build_mobile_libs.sh android

# 或者一次构建所有平台
./build_mobile_libs.sh all
```

### 示例 5: 模拟器测试

目标：在 Android x86 模拟器上测试

```bash
# 构建 x86_64 架构
./build_mobile_libs.sh android:x86_64

# 在 x86 模拟器上运行和测试
```

## 🔗 相关文档

- **[README_MOBILE.md](README_MOBILE.md)** - 移动平台支持概览
- **[MOBILE_BUILD_GUIDE.md](MOBILE_BUILD_GUIDE.md)** - 完整构建指南
- **[MOBILE_INTEGRATION_EXAMPLE.md](MOBILE_INTEGRATION_EXAMPLE.md)** - 集成示例

## 💡 最佳实践

### 1. 开发时优化构建时间

```bash
# ✅ 推荐：只构建需要的架构
./build_mobile_libs.sh android:arm64-v8a

# ❌ 不推荐：每次都构建所有架构
./build_mobile_libs.sh android  # 浪费 6 分钟
```

### 2. 使用增量构建

```bash
# 第一次完整构建
./build_mobile_libs.sh all

# 之后只重新构建修改的平台
./build_mobile_libs.sh ios    # 只重建 iOS
./build_mobile_libs.sh macos  # 只重建 macOS
```

### 3. CI/CD 集成

```yaml
# GitHub Actions 示例
- name: Build mobile libraries
  run: |
    ./build_mobile_libs.sh all

- name: Upload artifacts
  uses: actions/upload-artifact@v2
  with:
    name: mobile-libs
    path: mobile_libs/
```

### 4. 自动化脚本

```bash
#!/bin/bash
# my-build.sh - 自定义构建脚本

# 开发模式：快速构建
if [ "$1" == "dev" ]; then
    ./build_mobile_libs.sh android:arm64-v8a

# 发布模式：完整构建
elif [ "$1" == "release" ]; then
    ./build_mobile_libs.sh all
fi
```

---

**版本**: 1.2
**最后更新**: 2025-11-06
**新增功能**: macOS 平台支持、单架构构建支持
