# Mobile Platform Build Guide

## 📱 概述

本指南介绍如何为 iOS、macOS 和 Android 平台构建 `libquiche_engine` 库。

### 平台特定库策略

| 平台 | 库类型 | 包含内容 | 输出文件 |
|------|--------|----------|----------|
| **iOS** | 静态库 (.a) | libquiche.a + libev.a + C++ Engine | `libquiche_engine.a` |
| **macOS** | 静态库 (.a) | libquiche.a + libev.a + C++ Engine | `libquiche_engine.a` |
| **Android** | 动态库 (.so) | libquiche.a + libev.a + C++ Engine | `libquiche_engine.so` |

## 🚀 快速开始

### 使用自动构建脚本

```bash
# 构建 iOS 库
./build_mobile_libs.sh ios                     # arm64 (真机)
./build_mobile_libs.sh ios:x86_64              # x86_64 (模拟器)

# 构建 macOS 库
./build_mobile_libs.sh macos                   # 自动检测架构
./build_mobile_libs.sh macos:arm64             # Apple Silicon
./build_mobile_libs.sh macos:x86_64            # Intel Mac

# 构建 Android 库（所有架构）
./build_mobile_libs.sh android

# 构建 Android 单个架构（推荐：更快）
./build_mobile_libs.sh android:arm64-v8a      # 只构建 arm64-v8a
./build_mobile_libs.sh android:armeabi-v7a    # 只构建 armeabi-v7a
./build_mobile_libs.sh android:x86            # 只构建 x86
./build_mobile_libs.sh android:x86_64         # 只构建 x86_64

# 构建多个平台和架构
./build_mobile_libs.sh ios macos android:arm64-v8a

# 构建所有平台和架构
./build_mobile_libs.sh all
```

**💡 提示**:
- 开发阶段推荐只构建需要的架构，例如 `android:arm64-v8a`，可以大幅缩短构建时间
- 发布前使用 `android` 构建所有架构，确保所有设备兼容性

### 输出目录

```
mobile_libs/
├── ios/
│   ├── arm64/
│   │   └── libquiche_engine.a          # iOS 真机静态库
│   ├── x86_64/
│   │   └── libquiche_engine.a          # iOS 模拟器静态库
│   └── include/
│       └── quiche_engine.h              # 头文件
├── macos/
│   ├── arm64/
│   │   └── libquiche_engine.a          # Apple Silicon 静态库
│   ├── x86_64/
│   │   └── libquiche_engine.a          # Intel Mac 静态库
│   └── include/
│       └── quiche_engine.h              # 头文件
└── android/
    ├── arm64-v8a/
    │   └── libquiche_engine.so          # ARM64 动态库
    ├── armeabi-v7a/
    │   └── libquiche_engine.so          # ARMv7 动态库
    ├── x86/
    │   └── libquiche_engine.so          # x86 动态库
    ├── x86_64/
    │   └── libquiche_engine.so          # x86_64 动态库
    └── include/
        └── quiche_engine.h              # 头文件
```

## 📋 前置要求

### 通用要求

- **Rust** (1.83+)
- **Cargo**
- **Git** (with submodules)

### iOS 构建要求

- **macOS** (必须)
- **Xcode** (Command Line Tools)
- **iOS targets**: `aarch64-apple-ios`, `x86_64-apple-ios`

安装 iOS targets：
```bash
rustup target add aarch64-apple-ios      # 真机
rustup target add x86_64-apple-ios       # 模拟器
```

### macOS 构建要求

- **macOS** (必须)
- **Xcode** (Command Line Tools)
- **macOS targets**: `aarch64-apple-darwin`, `x86_64-apple-darwin`

安装 macOS targets：
```bash
rustup target add aarch64-apple-darwin   # Apple Silicon
rustup target add x86_64-apple-darwin    # Intel Mac
```

**💡 提示**:
- macOS targets 通常已默认安装
- 使用 `rustup target list | grep darwin` 查看已安装的 targets
- 使用 `uname -m` 查看当前系统架构

### Android 构建要求

- **Android NDK** (r21+, 推荐 r23)
- **ANDROID_NDK_HOME** 环境变量

设置 Android NDK：
```bash
export ANDROID_NDK_HOME=/path/to/android-ndk
# 例如：export ANDROID_NDK_HOME=$HOME/Library/Android/sdk/ndk/23.2.8568313
```

安装 Android targets：
```bash
rustup target add aarch64-linux-android
rustup target add armv7-linux-androideabi
rustup target add i686-linux-android
rustup target add x86_64-linux-android
```

## 🔧 手动构建步骤

### iOS 构建（手动）

```bash
cd quiche

# 1. 构建 quiche 库（带 cpp-engine 特性）
cargo build --lib --release \
    --target aarch64-apple-ios \
    --features cpp-engine

# 2. 找到构建输出目录
BUILD_OUT=$(find target/aarch64-apple-ios/release/build -name "quiche-*" -type d | head -1)/out

# 3. 合并库文件
libtool -static -o libquiche_engine.a \
    target/aarch64-apple-ios/release/libquiche.a \
    $BUILD_OUT/libev.a \
    $BUILD_OUT/libquiche_engine_fat.a
```

### Android 构建（手动）

```bash
cd quiche

# 设置环境变量
export ANDROID_NDK_HOME=/path/to/ndk
export ANDROID_API_LEVEL=21

# 以 ARM64 为例
TARGET=aarch64-linux-android
ABI=arm64-v8a

# 1. 构建 quiche 库
cargo build --lib --release \
    --target $TARGET \
    --features cpp-engine

# 2. 找到构建输出
BUILD_OUT=$(find target/$TARGET/release/build -name "quiche-*" -type d | head -1)/out

# 3. 创建共享库
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang++ \
    -shared \
    -o libquiche_engine.so \
    -Wl,--whole-archive \
    target/$TARGET/release/libquiche.a \
    $BUILD_OUT/libev.a \
    $BUILD_OUT/libquiche_engine.a \
    -Wl,--no-whole-archive \
    -lc++_shared \
    -llog \
    -lm
```

## 🏗️ build.rs 工作原理

### Android 平台

在 `build.rs` 中，当检测到 Android 平台时：

```rust
"android" => {
    // 1. 编译 libev.a
    libev_build.compile("ev");

    // 2. 编译 C++ Engine 为 libquiche_engine.a
    build.compile("quiche_engine");

    // 3. 使用 NDK 链接器创建 .so
    //    包含: libquiche.a + libev.a + libquiche_engine.a
    let so_output = out_path.join("libquiche_engine.so");
    let link_result = std::process::Command::new(&ndk_clang)
        .arg("-shared")
        .arg("-Wl,--whole-archive")
        .arg(&libengine_path)
        .arg(&libev_path)
        .arg("-Wl,--no-whole-archive")
        // ... 其他标志
        .output();
}
```

### iOS 平台

在 `build.rs` 中，当检测到 iOS 平台时：

```rust
"ios" => {
    // 1. 编译 libev.a
    libev_build.compile("ev");

    // 2. 编译 C++ Engine 为 libquiche_engine.a
    build.compile("quiche_engine");

    // 3. 使用 libtool 合并静态库
    //    包含: libev.a + libquiche_engine.a
    let combined_output = out_path.join("libquiche_engine_fat.a");
    let libtool_result = std::process::Command::new("libtool")
        .arg("-static")
        .arg("-o")
        .arg(&combined_output)
        .arg(&libengine_path)
        .arg(&libev_path)
        .output();
}
```

**注意**：iOS 的 fat 库在这个阶段只包含 libev 和 C++ Engine。最终应用需要同时链接 `libquiche.a` 和 `libquiche_engine_fat.a`，或者使用 `build_mobile_libs.sh` 脚本创建的包含所有内容的单一库。

## 📦 在移动应用中使用

### iOS (Swift/Objective-C)

#### 1. 添加库到 Xcode 项目

```
YourApp.xcodeproj
└── Frameworks/
    ├── libquiche_engine.a
    └── Headers/
        └── quiche_engine.h
```

#### 2. 配置 Build Settings

- **Library Search Paths**: `$(PROJECT_DIR)/Frameworks`
- **Header Search Paths**: `$(PROJECT_DIR)/Frameworks/Headers`
- **Other Linker Flags**: `-lc++ -lresolv`

#### 3. 在代码中使用

**Bridging Header** (for Swift):
```objc
// YourApp-Bridging-Header.h
#import "quiche_engine.h"
```

**Objective-C**:
```objc
#import "quiche_engine.h"

// 使用 C++ Engine
// ...
```

### Android (Java/Kotlin + JNI)

#### 1. 添加库到项目

```
app/
└── src/
    └── main/
        ├── jniLibs/
        │   ├── arm64-v8a/
        │   │   └── libquiche_engine.so
        │   ├── armeabi-v7a/
        │   │   └── libquiche_engine.so
        │   ├── x86/
        │   │   └── libquiche_engine.so
        │   └── x86_64/
        │       └── libquiche_engine.so
        └── cpp/
            └── include/
                └── quiche_engine.h
```

#### 2. 在 build.gradle 中配置

```gradle
android {
    // ...

    defaultConfig {
        // ...
        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
        }
    }

    sourceSets {
        main {
            jniLibs.srcDirs = ['src/main/jniLibs']
        }
    }
}
```

#### 3. 加载库

**Kotlin**:
```kotlin
class QuicheEngine {
    companion object {
        init {
            System.loadLibrary("quiche_engine")
        }
    }

    // Native methods
    external fun connect(host: String, port: Int): Boolean
    // ...
}
```

#### 4. CMakeLists.txt (如果使用 CMake)

```cmake
cmake_minimum_required(VERSION 3.18.1)

project("yourapp")

# 添加预构建的库
add_library(quiche_engine SHARED IMPORTED)
set_target_properties(quiche_engine PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libquiche_engine.so
)

# 你的 JNI 包装库
add_library(yourapp SHARED
    yourapp_jni.cpp
)

# 链接
target_link_libraries(yourapp
    quiche_engine
    log
    android
)
```

## 🔍 验证构建

### iOS 库验证

```bash
# 检查架构
lipo -info mobile_libs/ios/arm64/libquiche_engine.a
# 输出: Non-fat file: ... is architecture: arm64

# 检查符号
nm mobile_libs/ios/arm64/libquiche_engine.a | grep QuicheEngine | head -10

# 检查大小
du -h mobile_libs/ios/arm64/libquiche_engine.a
```

### Android 库验证

```bash
# 检查架构
file mobile_libs/android/arm64-v8a/libquiche_engine.so
# 输出: ELF 64-bit LSB shared object, ARM aarch64...

# 检查依赖
readelf -d mobile_libs/android/arm64-v8a/libquiche_engine.so | grep NEEDED
# 输出应包含: libc++_shared.so, liblog.so, libm.so

# 检查符号
nm -D mobile_libs/android/arm64-v8a/libquiche_engine.so | grep QuicheEngine | head -10

# 检查大小
du -h mobile_libs/android/arm64-v8a/libquiche_engine.so
```

## ⚠️ 常见问题

### Q: iOS 构建失败 "libtool: can't locate file"

**A**: 确保已正确构建：
```bash
cargo build --lib --release --target aarch64-apple-ios --features cpp-engine
```

### Q: Android 构建找不到 NDK 编译器

**A**: 检查 ANDROID_NDK_HOME 和主机平台：
```bash
echo $ANDROID_NDK_HOME
ls $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/
# 应该看到 darwin-x86_64 (macOS) 或 linux-x86_64 (Linux)
```

### Q: Android 应用运行时找不到 libc++_shared.so

**A**: 两种解决方案：

**方案1**: 包含 libc++_shared.so
```bash
cp $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so \
   app/src/main/jniLibs/arm64-v8a/
```

**方案2**: 静态链接 libc++
修改构建脚本使用 `-lc++_static` 而不是 `-lc++_shared`

### Q: 库太大，如何减小？

**A**:
1. 使用 Release 构建（已包含优化）
2. 使用 strip 移除符号：
   ```bash
   # iOS
   strip -x libquiche_engine.a

   # Android
   $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip \
       libquiche_engine.so
   ```

## 📊 预期库大小

| 平台 | 架构 | Debug 大小 | Release 大小 | Stripped |
|------|------|------------|--------------|----------|
| iOS | arm64 | ~8 MB | ~3 MB | ~2 MB |
| Android | arm64-v8a | ~6 MB | ~2 MB | ~1.5 MB |
| Android | armeabi-v7a | ~5 MB | ~1.8 MB | ~1.3 MB |

## 🎯 最佳实践

1. **使用 Release 构建**：生产环境必须使用 `--release` 标志

2. **Strip 符号**：发布版本移除调试符号

3. **测试所有架构**：
   - iOS: 测试真机（arm64）和模拟器（x86_64）
   - Android: 测试所有主要架构

4. **版本管理**：在库文件名中包含版本号
   ```
   libquiche_engine-v0.24.6.a
   libquiche_engine-v0.24.6.so
   ```

5. **CI/CD 集成**：将构建脚本集成到 CI 流程中

## 📚 相关文档

- **ENGINE_WITH_VENDORED_LIBEV.md** - 技术细节和架构
- **QUICK_START_ENGINE.md** - API 使用指南
- **engine/include/quiche_engine.h** - API 参考

## 🆘 获取帮助

如有问题：
1. 查看完整的构建日志
2. 检查环境变量配置
3. 验证工具链版本
4. 提交 Issue 并附带详细信息

---

**构建脚本版本**: 1.0
**最后更新**: 2025-11-06
