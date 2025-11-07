# Quiche Engine 构建说明

本文档说明如何构建 Quiche Engine 移动平台库以及已知问题的解决方案。

## 快速开始

### 构建 Android

```bash
# 设置 NDK 环境变量
export ANDROID_NDK_HOME=/path/to/ndk/21.4.7075529

# 构建 ARM64
./quiche_engine_all.sh android:arm64-v8a

# 构建所有 Android 架构
./quiche_engine_all.sh android
```

### 构建 iOS

```bash
# 构建 ARM64 (真机)
./quiche_engine_all.sh ios:arm64

# 构建 x86_64 (模拟器)
./quiche_engine_all.sh ios:x86_64
```

### 构建 macOS

```bash
# 构建 Apple Silicon
./quiche_engine_all.sh macos:arm64

# 构建 Intel
./quiche_engine_all.sh macos:x86_64
```

### 构建所有平台

```bash
./quiche_engine_all.sh all
```

## 输出结构

构建完成后，输出目录结构如下：

```
libs/
├── android/
│   ├── arm64-v8a/
│   │   └── libquiche_engine.so
│   ├── armeabi-v7a/
│   │   └── libquiche_engine.so
│   ├── x86/
│   │   └── libquiche_engine.so
│   └── x86_64/
│       └── libquiche_engine.so
├── ios/
│   ├── arm64/
│   │   └── libquiche_engine.a
│   └── x86_64/
│       └── libquiche_engine.a
├── macos/
│   ├── arm64/
│   │   └── libquiche_engine.a
│   └── x86_64/
│       └── libquiche_engine.a
└── include/
    └── quiche_engine.h
```

## 已知问题和解决方案

### 1. Android NDK 21 的 libunwind 问题

**问题描述：**
在使用 NDK 21 构建 Android 库时，链接器会报错找不到 `-lunwind`：

```
ld: cannot find -lunwind
clang: error: linker command failed with exit code 1
```

**原因：**
在 NDK 21 中，`libunwind` 的功能已经集成在 `libgcc.a` 中，但 Rust 标准库仍然尝试链接独立的 `libunwind`。

**解决方案：**
为每个 Android 架构创建 `libunwind.a` 符号链接，指向 `libgcc.a`：

```bash
# ARM64
ln -sf ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/lib/gcc/aarch64-linux-android/4.9.x/libgcc.a \
       ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/21/libunwind.a

# ARMv7
ln -sf ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/lib/gcc/arm-linux-androideabi/4.9.x/libgcc.a \
       ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/arm-linux-androideabi/21/libunwind.a

# x86
ln -sf ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/lib/gcc/i686-linux-android/4.9.x/libgcc.a \
       ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/i686-linux-android/21/libunwind.a

# x86_64
ln -sf ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/lib/gcc/x86_64-linux-android/4.9.x/libgcc.a \
       ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/x86_64-linux-android/21/libunwind.a
```

**注意：** 只需要执行一次，不需要每次构建都执行。

### 2. iOS/macOS 的 nm 工具版本不兼容

**问题描述：**
构建 iOS 或 macOS 库时，Xcode 的 `nm` 工具报错：

```
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/nm: error:
Unknown attribute kind (91) (Producer: 'LLVM19.1.1-rust-1.83.0-stable' Reader: 'LLVM APPLE_1_1600.0.26.6_0')
```

**原因：**
Rust 1.83 使用 LLVM 19.1.1 编译，而 Xcode 的 `nm` 工具使用的是较老的 LLVM 版本，无法读取新格式的目标文件。

**解决方案：**
已在构建脚本中修复，现在会：
1. 优先使用 Rust 工具链中的 `llvm-nm`（如果可用）
2. 如果 `llvm-nm` 不可用，跳过符号检查（不影响库的正常使用）

这个错误不影响库的功能，只是无法在构建时显示符号信息。你可以在构建后手动验证符号：

```bash
# 使用 otool 查看符号
otool -tV libs/ios/arm64/libquiche_engine.a | grep QuicheEngine

# 或使用 nm（忽略错误）
nm -g libs/ios/arm64/libquiche_engine.a 2>/dev/null | grep QuicheEngine
```

### 3. libtool 的重复成员名警告

**问题描述：**
构建 iOS 库时，`libtool` 报告大量重复成员名警告：

```
libtool: warning duplicate member name 'x509_def.c.o' from ...
```

**原因：**
`libquiche.a` 和 `libcrypto.a`/`libssl.a` 中包含一些相同的目标文件（因为 BoringSSL 在构建时被链接了两次）。

**影响：**
这些警告不影响库的功能。`libtool` 会自动处理重复的符号，使用其中一个版本。

**解决方案：**
无需处理，这是正常现象。最终生成的库可以正常使用。

## 依赖和系统要求

### 通用要求
- Rust 1.83 or later
- cmake
- git（用于 submodule）

### macOS/iOS 构建
- Xcode Command Line Tools
- Xcode 14.0 or later

### Android 构建
- Android NDK 21.x (推荐) 或 23.x/25.x
- 设置 `ANDROID_NDK_HOME` 环境变量

## 构建选项

构建脚本支持以下选项：

```bash
# 单个平台/架构
./quiche_engine_all.sh ios:arm64
./quiche_engine_all.sh android:arm64-v8a
./quiche_engine_all.sh macos:x86_64

# 多个平台/架构（空格分隔）
./quiche_engine_all.sh ios:arm64 android:arm64-v8a macos:arm64

# 构建所有架构
./quiche_engine_all.sh ios              # iOS arm64
./quiche_engine_all.sh android          # 所有 Android 架构
./quiche_engine_all.sh macos            # 当前系统架构

# 构建所有平台
./quiche_engine_all.sh all
```

## 库说明

### Android (.so)
- **libquiche_engine.so**: 包含 QUIC/HTTP3 实现 + BoringSSL + libev + C++ Engine
- **大小**: ~10MB (arm64-v8a)
- **符号**: 1,000+ BoringSSL 符号

### iOS/macOS (.a)
- **libquiche_engine.a**: 包含 QUIC/HTTP3 实现 + BoringSSL + libev + C++ Engine
- **大小**: ~78MB (iOS arm64), ~45MB (macOS)
- **类型**: 静态库，包含所有依赖

## C++ API

所有平台共享相同的 C++ 头文件 `libs/include/quiche_engine.h`。

示例用法：

```cpp
#include "quiche_engine.h"

// 创建 QUIC 引擎配置
quiche::EngineConfig config;
config.set(quiche::ConfigKey::MAX_IDLE_TIMEOUT, 30000);
config.set(quiche::ConfigKey::INITIAL_MAX_DATA, 10000000);

// 创建 QUIC 引擎实例
auto engine = quiche::QuicheEngine::create(config);

// 启动事件循环
engine->run();
```

## 故障排除

### Git Submodule 未初始化

如果遇到 BoringSSL 相关错误：

```bash
git submodule update --init --recursive
```

构建脚本会自动检查并初始化 submodule，但手动初始化可以解决一些边缘情况。

### Cargo 配置问题

`.cargo/config.toml` 是自动生成的，不要手动编辑。如果遇到配置问题：

```bash
rm .cargo/config.toml
./quiche_engine_all.sh android:arm64-v8a  # 会重新生成
```

### 关于绝对路径的说明

**问**: 为什么 `.cargo/config.toml` 包含绝对路径（如 `/Users/jiangzhongyang/...`）？

**答**: 这是有意为之，**不是问题**：

1. ✅ **每次构建自动生成** - 基于当前开发者的 `$ANDROID_NDK_HOME`
2. ✅ **不提交到 Git** - 已在 `.gitignore` 中排除
3. ✅ **跨开发者兼容** - 每人生成自己的配置
4. ✅ **跨平台支持** - 自动检测 macOS/Linux

**原因**: Cargo 的 `[target.*]` 配置不支持环境变量展开和相对路径，绝对路径是唯一支持的格式。

详细说明请参见 [.cargo/DESIGN.md](.cargo/DESIGN.md)。

### 清理构建

```bash
# 清理所有构建产物
cargo clean
rm -rf libs .cargo/config.toml

# 重新构建
./quiche_engine_all.sh all
```

## 更多信息

- 项目架构: 参见 [CLAUDE.md](CLAUDE.md)
- Android NDK 配置: 参见 [.cargo/README.md](.cargo/README.md)
