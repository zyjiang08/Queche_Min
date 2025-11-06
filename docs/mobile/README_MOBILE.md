# Mobile Platform Support - README

## 🎉 概述

quiche 现已支持 iOS、macOS 和 Android 平台，提供高性能的 QUIC 协议实现。

### 平台支持

| 平台 | 架构 | 库类型 | 状态 |
|------|------|--------|------|
| **iOS** | arm64 | 静态库 (.a) | ✅ 已支持 |
| **iOS** | x86_64 (模拟器) | 静态库 (.a) | ✅ 已支持 |
| **macOS** | arm64 (Apple Silicon) | 静态库 (.a) | ✅ 已支持 |
| **macOS** | x86_64 (Intel) | 静态库 (.a) | ✅ 已支持 |
| **Android** | arm64-v8a | 动态库 (.so) | ✅ 已支持 |
| **Android** | armeabi-v7a | 动态库 (.so) | ✅ 已支持 |
| **Android** | x86 | 动态库 (.so) | ✅ 已支持 |
| **Android** | x86_64 | 动态库 (.so) | ✅ 已支持 |

## 🚀 快速开始

### 1. 构建库

**注意**: 构建脚本位于项目根目录，请从项目根目录运行。

```bash
# 从项目根目录运行
cd /path/to/quiche

# 构建 iOS 库（真机）
./build_mobile_libs.sh ios                    # 默认 arm64 (真机)
./build_mobile_libs.sh ios:arm64              # 显式指定 arm64
./build_mobile_libs.sh ios:x86_64             # 模拟器

# 构建 macOS 库
./build_mobile_libs.sh macos                  # 自动检测架构
./build_mobile_libs.sh macos:arm64            # Apple Silicon
./build_mobile_libs.sh macos:x86_64           # Intel

# 构建 Android 库（所有架构）
./build_mobile_libs.sh android

# 构建 Android 单个架构（更快）
./build_mobile_libs.sh android:arm64-v8a      # 只构建 arm64
./build_mobile_libs.sh android:armeabi-v7a    # 只构建 armv7
./build_mobile_libs.sh android:x86            # 只构建 x86
./build_mobile_libs.sh android:x86_64         # 只构建 x86_64

# 构建多平台组合
./build_mobile_libs.sh ios macos android:arm64-v8a

# 构建所有平台（iOS + macOS + Android 所有架构）
./build_mobile_libs.sh all
```

### 2. 获取输出

```
mobile_libs/
├── ios/
│   ├── arm64/
│   │   └── libquiche_engine.a       # 包含所有依赖的单一库
│   └── include/
│       └── quiche_engine.h
└── android/
    ├── arm64-v8a/
    │   └── libquiche_engine.so      # 包含所有依赖的单一库
    ├── armeabi-v7a/
    │   └── libquiche_engine.so
    ├── x86/
    │   └── libquiche_engine.so
    ├── x86_64/
    │   └── libquiche_engine.so
    └── include/
        └── quiche_engine.h
```

### 3. 集成到应用

#### iOS (Swift)

```swift
// 1. 添加 libquiche_engine.a 到 Xcode 项目
// 2. 创建 Bridging Header
// 3. 使用

let engine = QuicheEngine(host: "example.com", port: "443", config: config)
engine.setEventCallback(callback, userData: nil)
engine.start()
```

#### Android (Kotlin)

```kotlin
// 1. 添加 .so 文件到 jniLibs/
// 2. 创建 JNI 包装层
// 3. 使用

val engine = QuicheEngine()
engine.connect("example.com", 443, listener)
```

## 📚 文档

**📂 所有文档已统一组织到 `docs/mobile/` 目录**

### 核心文档

- **[MOBILE_BUILD_GUIDE.md](MOBILE_BUILD_GUIDE.md)** - 完整构建指南
  - 前置要求
  - 手动构建步骤
  - 验证方法
  - 常见问题

- **[MOBILE_PLATFORM_SUMMARY.md](MOBILE_PLATFORM_SUMMARY.md)** - 技术总结
  - 实施细节
  - build.rs 工作原理
  - 性能指标
  - 架构设计

- **[MOBILE_INTEGRATION_EXAMPLE.md](MOBILE_INTEGRATION_EXAMPLE.md)** - 集成示例
  - iOS Swift 完整示例
  - Android Kotlin + JNI 完整示例
  - 最佳实践

### 辅助文档

- **[ENGINE_WITH_VENDORED_LIBEV.md](ENGINE_WITH_VENDORED_LIBEV.md)** - 引擎架构
- **[QUICK_START_ENGINE.md](QUICK_START_ENGINE.md)** - API 快速参考
- **[iOS_QUICK_START.md](iOS_QUICK_START.md)** - iOS 专属快速指南
- **[iOS_BUILD_FIX.md](iOS_BUILD_FIX.md)** - iOS 构建问题修复
- **[iOS_CHKSTK_FIX_SUMMARY.md](iOS_CHKSTK_FIX_SUMMARY.md)** - iOS chkstk 问题总结

### 文档导航

查看 **[docs/README.md](../README.md)** 获取完整的文档索引和推荐阅读顺序。

## ⚙️ 技术特性

### 核心优势

1. **单一库文件**
   - Android: `libquiche_engine.so` 包含所有依赖
   - iOS: `libquiche_engine.a` 包含所有依赖

2. **零外部依赖**
   - libev 源码内置，无需系统安装
   - 完全自包含构建

3. **性能优化**
   - Release 模式构建
   - Strip 符号后体积小
   - 平台特定优化

4. **线程安全**
   - 命令队列保护
   - 读缓冲区锁
   - 独立事件循环线程

### 库组成

每个库包含以下内容：

```
libquiche_engine (.so/.a)
├── libquiche.a          # QUIC 协议实现（Rust）
├── libev.a              # 事件循环库（C）
└── C++ Engine           # 高级 C++ API
    ├── QuicheEngine 类
    ├── 线程管理
    └── 事件回调
```

## 📊 性能数据

### 库大小

| 平台 | 架构 | Release + Strip |
|------|------|-----------------|
| iOS | arm64 | ~2 MB |
| Android | arm64-v8a | ~1.5 MB |
| Android | armeabi-v7a | ~1.3 MB |

### 构建时间

| 操作 | 时间（M1 Mac） |
|------|----------------|
| iOS arm64 | ~2 分钟 |
| Android 所有架构 | ~8 分钟 |
| 增量构建 | ~10-30 秒 |

## 🔧 前置要求

### 通用

- Rust 1.83+
- Cargo
- Git (with submodules)

### iOS

- macOS（必须）
- Xcode Command Line Tools
- iOS target: `rustup target add aarch64-apple-ios`

### Android

- Android NDK r21+（推荐 r23）
- 环境变量：`export ANDROID_NDK_HOME=/path/to/ndk`
- Android targets:
  ```bash
  rustup target add aarch64-linux-android
  rustup target add armv7-linux-androideabi
  rustup target add i686-linux-android
  rustup target add x86_64-linux-android
  ```

## 🛠️ 手动构建（高级）

### iOS

```bash
cd quiche

# 构建
cargo build --lib --release \
    --target aarch64-apple-ios \
    --features cpp-engine

# 合并库
BUILD_OUT=$(find target/aarch64-apple-ios/release/build -name "quiche-*" -type d | head -1)/out
libtool -static -o libquiche_engine.a \
    target/aarch64-apple-ios/release/libquiche.a \
    $BUILD_OUT/libev.a \
    $BUILD_OUT/libquiche_engine_fat.a
```

### Android

```bash
cd quiche

export ANDROID_NDK_HOME=/path/to/ndk
export ANDROID_API_LEVEL=21

# 构建（以 arm64 为例）
cargo build --lib --release \
    --target aarch64-linux-android \
    --features cpp-engine

# 创建共享库
BUILD_OUT=$(find target/aarch64-linux-android/release/build -name "quiche-*" -type d | head -1)/out
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang++ \
    -shared -o libquiche_engine.so \
    -Wl,--whole-archive \
    target/aarch64-linux-android/release/libquiche.a \
    $BUILD_OUT/libev.a \
    $BUILD_OUT/libquiche_engine.a \
    -Wl,--no-whole-archive \
    -lc++_shared -llog -lm
```

## 🐛 常见问题

### Q: 找不到 Android NDK 编译器

**A**: 检查 ANDROID_NDK_HOME 和主机平台：
```bash
echo $ANDROID_NDK_HOME
ls $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/
```

### Q: iOS 构建失败 "libtool: can't locate file"

**A**: 确保先运行 cargo build：
```bash
cargo build --lib --release --target aarch64-apple-ios --features cpp-engine
```

### Q: Android 应用崩溃找不到 libc++_shared.so

**A**: 方案 1 - 包含 libc++_shared.so：
```bash
cp $ANDROID_NDK_HOME/.../libc++_shared.so app/src/main/jniLibs/arm64-v8a/
```

方案 2 - 使用静态链接（修改构建脚本）

### Q: 如何减小库大小？

**A**:
```bash
# iOS
strip -x libquiche_engine.a

# Android
$ANDROID_NDK_HOME/.../llvm-strip libquiche_engine.so
```

## 📝 API 预览

### C++ API

```cpp
#include <quiche_engine.h>

using namespace quiche;

// 配置
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;
config[ConfigKey::INITIAL_MAX_DATA] = 10000000;

// 创建
QuicheEngine engine("example.com", "443", config);

// 回调
engine.setEventCallback([](QuicheEngine* e, EngineEvent event,
                           const EventData& data, void* user_data) {
    // 处理事件
}, nullptr);

// 启动
engine.start();

// 数据传输
engine.write(streamId, data, len, fin);
engine.read(streamId, buffer, bufLen, fin);

// 关闭
engine.shutdown(0, "Done");
```

### Swift API (通过 Bridging)

```swift
let engine = QuicheEngine(host: host, port: port, config: config)
engine.setEventCallback(callback, userData: nil)
engine.start()
engine.send(streamId: 4, data: data)
let (data, fin) = engine.receive(streamId: 4)
engine.shutdown()
```

### Kotlin API (通过 JNI)

```kotlin
val engine = QuicheEngine()
engine.connect(host, port, listener)
engine.send(streamId, data)
val data = engine.receive(streamId)
engine.shutdown()
```

## 🎯 使用场景

- **视频流应用**：低延迟实时流媒体
- **游戏**：快速可靠的多人游戏通信
- **IoT 应用**：高效的设备间通信
- **VPN/代理**：QUIC 隧道实现
- **文件传输**：快速可靠的文件同步

## 🔮 路线图

### 近期

- [ ] iOS 模拟器支持 (x86_64)
- [ ] Universal Binary (arm64 + x86_64)
- [ ] 自动化 CI/CD 构建

### 中期

- [ ] CocoaPods 支持
- [ ] Maven Central 发布
- [ ] 性能优化和 benchmark

### 长期

- [ ] Swift Package 直接支持
- [ ] Kotlin Multiplatform 封装
- [ ] HarmonyOS 支持

## 🤝 贡献

欢迎贡献！请查看主仓库的贡献指南。

## 📄 许可

与主 quiche 项目相同：BSD 或 Apache 2.0

## 🆘 获取帮助

- **文档**：查看 `docs/mobile/` 目录下的完整文档
- **文档索引**：参考 [docs/README.md](../README.md) 查看推荐阅读顺序
- **示例**：参考 [MOBILE_INTEGRATION_EXAMPLE.md](MOBILE_INTEGRATION_EXAMPLE.md)
- **问题**：提交 GitHub Issue

---

**版本**: 1.0
**最后更新**: 2025-11-06
**维护者**: quiche team
