# Quiche Engine 编译指南

本文档提供 Quiche Engine 移动平台库的完整编译方法说明。

## 目录

- [快速开始](#快速开始)
- [命令格式](#命令格式)
- [编译示例](#编译示例)
- [产物说明](#产物说明)
- [平台和架构支持](#平台和架构支持)

## 快速开始

### 前置要求

**通用要求**:
- Rust 1.83 or later
- cmake
- git

**macOS/iOS**:
- Xcode Command Line Tools
- Xcode 14.0 or later

**Android**:
- Android NDK 21.x (推荐) 或 23.x/25.x
- 设置 `ANDROID_NDK_HOME` 环境变量

### 一行命令编译

```bash
# iOS 真机
./quiche_engine_all.sh ios arm64

# macOS Apple Silicon
./quiche_engine_all.sh macos arm64

# Android ARM64
export ANDROID_NDK_HOME=/path/to/ndk
./quiche_engine_all.sh android arm64-v8a
```

## 命令格式

### 基本语法

```bash
./quiche_engine_all.sh <platform> [arch] [<platform> [arch] ...]
```

### 参数说明

| 参数 | 说明 |
|------|------|
| `platform` | 目标平台：`ios`, `macos`, `android`, `all` |
| `arch` | 目标架构（可选），省略时使用默认值 |
| `all` | 特殊关键字，构建该平台所有架构 |

### 默认行为

如果省略 `arch` 参数：

| 平台 | 默认行为 |
|------|---------|
| `ios` | 构建 `arm64` (真机) |
| `macos` | 构建当前系统架构（自动检测）|
| `android` | 构建所有架构（arm64-v8a, armeabi-v7a, x86, x86_64）|

## 编译示例

### iOS 平台

```bash
# 编译 iOS 真机 (arm64)
./quiche_engine_all.sh ios arm64

# 编译 iOS 模拟器 (x86_64)
./quiche_engine_all.sh ios x86_64

# 编译所有 iOS 架构
./quiche_engine_all.sh ios all

# 省略架构 - 默认 arm64
./quiche_engine_all.sh ios
```

### macOS 平台

```bash
# 编译 macOS Apple Silicon (M1/M2/M3)
./quiche_engine_all.sh macos arm64

# 编译 macOS Intel
./quiche_engine_all.sh macos x86_64

# 编译所有 macOS 架构
./quiche_engine_all.sh macos all

# 省略架构 - 自动检测当前系统
./quiche_engine_all.sh macos
```

### Android 平台

```bash
# 设置 NDK 路径
export ANDROID_NDK_HOME=/Users/你的用户名/Library/Android/sdk/ndk/21.4.7075529

# 编译 ARM64 (最常用)
./quiche_engine_all.sh android arm64-v8a

# 编译 ARMv7
./quiche_engine_all.sh android armeabi-v7a

# 编译 x86 (模拟器)
./quiche_engine_all.sh android x86

# 编译 x86_64 (模拟器)
./quiche_engine_all.sh android x86_64

# 编译所有 Android 架构
./quiche_engine_all.sh android all

# 省略架构 - 默认编译所有
./quiche_engine_all.sh android
```

### 多平台编译

```bash
# iOS + Android
./quiche_engine_all.sh ios arm64 android arm64-v8a

# iOS + macOS
./quiche_engine_all.sh ios arm64 macos arm64

# iOS + macOS + Android
./quiche_engine_all.sh ios arm64 macos arm64 android arm64-v8a

# 所有平台（使用默认架构）
./quiche_engine_all.sh all
```

### 高级用法

```bash
# 编译所有 iOS 架构 + 所有 Android 架构
./quiche_engine_all.sh ios all android all

# 编译多个 iOS 架构 + 特定 Android 架构
./quiche_engine_all.sh ios arm64 ios x86_64 android arm64-v8a

# 编译所有 macOS 架构 + 所有 Android 架构
./quiche_engine_all.sh macos all android all
```

## 产物说明

### 目录结构

编译完成后，产物存放在以下目录：

```
项目根目录/
├── lib/                    # 库文件目录
│   ├── ios/
│   │   ├── arm64/
│   │   │   └── libquiche_engine.a
│   │   └── x86_64/
│   │       └── libquiche_engine.a
│   ├── macos/
│   │   ├── arm64/
│   │   │   └── libquiche_engine.a
│   │   └── x86_64/
│   │       └── libquiche_engine.a
│   └── android/
│       ├── arm64-v8a/
│       │   └── libquiche_engine.so
│       ├── armeabi-v7a/
│       │   └── libquiche_engine.so
│       ├── x86/
│       │   └── libquiche_engine.so
│       └── x86_64/
│           └── libquiche_engine.so
└── include/                # 头文件目录（所有平台共享）
    └── quiche_engine.h
```

### 库文件说明

| 平台 | 文件类型 | 文件名 | 大小 | 说明 |
|------|---------|--------|------|------|
| iOS | 静态库 | `libquiche_engine.a` | ~78MB | 包含 BoringSSL + libev |
| macOS | 静态库 | `libquiche_engine.a` | ~45MB | 包含 BoringSSL + libev |
| Android | 动态库 | `libquiche_engine.so` | ~10MB | 包含 BoringSSL + libev |

### 库内容

所有库文件都包含：
- ✅ QUIC/HTTP3 协议实现
- ✅ BoringSSL 加密库 (1000+ 符号)
- ✅ libev 事件循环库
- ✅ C++ Engine 高级 API

### 头文件说明

`include/quiche_engine.h` - C++ API 头文件，所有平台共享。

主要接口：
```cpp
namespace quiche {
    class QuicheEngine;
    class EngineConfig;
    enum class ConfigKey;
    struct ConfigValue;
}
```

## 平台和架构支持

### iOS

| 架构 | 目标 | 设备类型 |
|------|------|---------|
| `arm64` | `aarch64-apple-ios` | iPhone, iPad (真机) |
| `x86_64` | `x86_64-apple-ios` | iOS Simulator |

### macOS

| 架构 | 目标 | 硬件 |
|------|------|------|
| `arm64` | `aarch64-apple-darwin` | Apple Silicon (M1/M2/M3/M4) |
| `x86_64` | `x86_64-apple-darwin` | Intel Mac |

### Android

| 架构 | ABI | 目标 | 说明 |
|------|-----|------|------|
| `arm64-v8a` | arm64-v8a | `aarch64-linux-android` | 64-bit ARM（最常用）|
| `armeabi-v7a` | armeabi-v7a | `armv7-linux-androideabi` | 32-bit ARM |
| `x86` | x86 | `i686-linux-android` | 32-bit x86 模拟器 |
| `x86_64` | x86_64 | `x86_64-linux-android` | 64-bit x86 模拟器 |

## 使用产物

### 集成到项目

1. **复制文件到项目**:
   ```bash
   cp -r lib/ /path/to/your/project/
   cp -r include/ /path/to/your/project/
   ```

2. **iOS/macOS (Xcode)**:
   - Add `lib/ios/arm64/libquiche_engine.a` to "Link Binary With Libraries"
   - Add `include/` to "Header Search Paths"

3. **Android (CMakeLists.txt)**:
   ```cmake
   add_library(quiche_engine SHARED IMPORTED)
   set_target_properties(quiche_engine PROPERTIES
       IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/android/${ANDROID_ABI}/libquiche_engine.so
   )

   include_directories(${CMAKE_SOURCE_DIR}/include)
   target_link_libraries(your_target quiche_engine)
   ```

### 代码示例

```cpp
#include "quiche_engine.h"

// 创建配置
quiche::EngineConfig config;
config.set(quiche::ConfigKey::MAX_IDLE_TIMEOUT, 30000);
config.set(quiche::ConfigKey::INITIAL_MAX_DATA, 10000000);

// 创建引擎实例
auto engine = quiche::QuicheEngine::create(config);

// 启动事件循环
engine->run();
```

## 清理构建

```bash
# 清理所有构建产物
cargo clean
rm -rf lib include .cargo/config.toml

# 重新编译
./quiche_engine_all.sh ios arm64
```

## 故障排除

### 问题: BoringSSL 子模块未初始化

```bash
git submodule update --init --recursive
```

### 问题: Android NDK 路径未设置

```bash
# macOS
export ANDROID_NDK_HOME=/Users/$(whoami)/Library/Android/sdk/ndk/21.4.7075529

# Linux
export ANDROID_NDK_HOME=/home/$(whoami)/Android/Sdk/ndk/21.4.7075529
```

### 问题: Android 编译失败 - cannot find -lunwind

这是 NDK 21 的已知问题，需要创建符号链接：

```bash
# 详见 README_BUILD.md 中的解决方案
ln -sf ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/lib/gcc/aarch64-linux-android/4.9.x/libgcc.a \
       ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/21/libunwind.a
```

### 问题: iOS/macOS nm 工具报错

这不是真正的错误，是工具版本不兼容。库文件可以正常使用。

## 更多信息

- 详细构建说明: [README_BUILD.md](README_BUILD.md)
- 项目架构: [CLAUDE.md](CLAUDE.md)
- Cargo 配置: [.cargo/DESIGN.md](.cargo/DESIGN.md)

## 支持

如有问题，请查看：
1. [README_BUILD.md](README_BUILD.md) - 已知问题和解决方案
2. [.cargo/DESIGN.md](.cargo/DESIGN.md) - 配置设计说明
