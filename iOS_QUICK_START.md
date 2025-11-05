# iOS 快速开始指南

## 🚀 一键编译

### 自动编译脚本

我们提供了一个自动化脚本来编译所有 iOS 平台的库：

```bash
./build_ios.sh
```

这个脚本会：
1. ✅ 检查并安装所需的 iOS 编译目标
2. ✅ 自动编译所有架构（ARM64 设备、ARM64 模拟器、x86_64 模拟器）
3. ✅ 创建通用模拟器库
4. ✅ 生成 XCFramework（可直接拖入 Xcode）
5. ✅ 验证 `__chkstk_darwin` 符号修复

### 输出文件

编译成功后，会在以下目录生成文件：

```
ios-libs/
├── libquiche-ios-arm64.a         # iOS 设备库
├── libquiche-sim-arm64.a         # 模拟器 ARM64 库
├── libquiche-sim-x86_64.a        # 模拟器 x86_64 库
└── libquiche-simulator.a         # 通用模拟器库

libquiche.xcframework/            # XCFramework（推荐使用）
```

## 📱 集成到 Xcode 项目

### 方法 1：使用 XCFramework（推荐）

1. **添加 Framework**
   - 将 `libquiche.xcframework` 拖入 Xcode 项目
   - 在 "General" → "Frameworks, Libraries, and Embedded Content" 中确认已添加

2. **添加系统依赖**

   在 "Build Phases" → "Link Binary With Libraries" 中添加：
   - `Security.framework`
   - `libresolv.tbd`

3. **配置头文件**

   在 "Build Settings" 中：
   - **Header Search Paths**: 添加 `$(PROJECT_DIR)/../quiche/include`
   - 或者将 `quiche.h` 复制到项目中

### 方法 2：使用静态库

1. **添加静态库**
   - iOS 设备：使用 `libquiche-ios-arm64.a`
   - 模拟器：使用 `libquiche-simulator.a`

2. **配置 Build Settings**
   ```
   Library Search Paths: $(PROJECT_DIR)/ios-libs
   Header Search Paths: $(PROJECT_DIR)/../quiche/include
   Other Linker Flags: -lquiche -lresolv
   ```

3. **添加框架**
   - `Security.framework`
   - `libresolv.tbd`

## 🛠️ 手动编译

如果需要手动编译特定架构：

### iOS 设备 (ARM64)

```bash
cargo build --release \
  --target aarch64-apple-ios \
  --no-default-features \
  --features ffi,boringssl-vendored
```

输出：`target/aarch64-apple-ios/release/libquiche.a`

### iOS 模拟器 (ARM64)

```bash
cargo build --release \
  --target aarch64-apple-ios-sim \
  --no-default-features \
  --features ffi,boringssl-vendored
```

输出：`target/aarch64-apple-ios-sim/release/libquiche.a`

### iOS 模拟器 (x86_64)

```bash
cargo build --release \
  --target x86_64-apple-ios \
  --no-default-features \
  --features ffi,boringssl-vendored
```

输出：`target/x86_64-apple-ios/release/libquiche.a`

## 💻 使用示例

### Swift 代码示例

```swift
import Foundation

// 导入 C 头文件（需要创建 bridging header）
// #import "quiche.h"

class QUICClient {
    func connect(to host: String, port: UInt16) {
        // 创建配置
        let config = quiche_config_new(0xbabababa)

        // 设置参数
        quiche_config_set_max_idle_timeout(config, 5000)
        quiche_config_set_max_recv_udp_payload_size(config, 1350)
        quiche_config_set_initial_max_data(config, 10000000)

        // ... 使用 quiche API

        // 清理
        quiche_config_free(config)
    }
}
```

### Objective-C 代码示例

```objc
#import "quiche.h"

@implementation QUICClient

- (void)connectToHost:(NSString *)host port:(uint16_t)port {
    // 创建配置
    quiche_config *config = quiche_config_new(0xbabababa);

    // 设置参数
    quiche_config_set_max_idle_timeout(config, 5000);
    quiche_config_set_max_recv_udp_payload_size(config, 1350);
    quiche_config_set_initial_max_data(config, 10000000);

    // ... 使用 quiche API

    // 清理
    quiche_config_free(config);
}

@end
```

### 创建 Bridging Header (Swift 项目)

1. 创建 `ProjectName-Bridging-Header.h`：
   ```objc
   #ifndef ProjectName_Bridging_Header_h
   #define ProjectName_Bridging_Header_h

   #import "quiche.h"

   #endif
   ```

2. 在 Build Settings 中设置：
   ```
   Objective-C Bridging Header: $(PROJECT_DIR)/ProjectName-Bridging-Header.h
   ```

## 🔍 验证编译结果

### 检查库文件

```bash
# 查看文件大小
ls -lh ios-libs/

# 查看架构信息
lipo -info ios-libs/libquiche-ios-arm64.a
lipo -info ios-libs/libquiche-simulator.a

# 查看导出符号
nm -g ios-libs/libquiche-ios-arm64.a | grep " T _quiche" | head -20
```

### 验证 __chkstk_darwin 修复

```bash
# 检查是否包含 __chkstk_darwin 符号
nm -g ios-libs/libquiche-ios-arm64.a | grep "__chkstk_darwin"

# 应该看到类似输出：
# 0000000000000000 T ___chkstk_darwin
```

## ⚙️ 编译选项说明

### 必需的 Features

- `ffi` - 启用 C FFI 接口
- `boringssl-vendored` - 使用内置的 BoringSSL

### 可选的 Features

```bash
# 启用 qlog 日志
--features ffi,boringssl-vendored,qlog

# 使用 Google 拥塞控制
--features ffi,boringssl-vendored,gcongestion
```

## 🐛 常见问题

### Q1: 编译时报 "__chkstk_darwin undefined" 错误

**解决方案：** 确认以下文件存在：
- ✅ `quiche/chkstk_darwin.c`
- ✅ `quiche/src/build.rs` 包含编译 `chkstk_darwin.c` 的代码
- ✅ `quiche/Cargo.toml` 的 `[build-dependencies]` 包含 `cc = "1.0"`

如果文件齐全但仍有错误，尝试清理重新编译：
```bash
cargo clean
./build_ios.sh
```

### Q2: Xcode 16.2 链接错误

如果遇到新版 Xcode 的链接问题，在 "Other Linker Flags" 中添加：
```
-Wl,-ld_classic
```

这会使用旧版链接器。

### Q3: 模拟器无法运行

确保使用正确的库：
- **M1/M2 Mac**: 使用 `aarch64-apple-ios-sim`
- **Intel Mac**: 使用 `x86_64-apple-ios`
- **通用**: 使用 `libquiche-simulator.a` (包含两种架构)

### Q4: bitcode 相关错误

iOS 15+ 已不再需要 bitcode，如果遇到相关错误：

在 Xcode Build Settings 中：
```
Enable Bitcode: NO
```

### Q5: 编译速度慢

BoringSSL 编译需要时间（约 2-5 分钟）。可以使用以下方法加速：

1. **增加并行编译**：
   ```bash
   export CARGO_BUILD_JOBS=8
   ./build_ios.sh
   ```

2. **只编译需要的架构**：
   ```bash
   # 只编译设备版本
   cargo build --release --target aarch64-apple-ios \
     --no-default-features --features ffi,boringssl-vendored
   ```

## 📊 库大小参考

编译后的库大小（release 模式）：

| 架构 | 大小 | 说明 |
|------|------|------|
| aarch64-apple-ios | ~6-8 MB | iOS 设备（iPhone/iPad） |
| aarch64-apple-ios-sim | ~6-8 MB | iOS 模拟器（M1+ Mac） |
| x86_64-apple-ios | ~6-8 MB | iOS 模拟器（Intel Mac） |
| Universal Simulator | ~12-15 MB | 包含 ARM64 + x86_64 |
| XCFramework | ~18-22 MB | 包含所有架构 |

**注意：** 实际 App 大小只会包含当前设备架构的代码，不会包含所有架构。

## 🎯 最佳实践

### 1. 使用 XCFramework

XCFramework 是 Apple 推荐的方式，自动选择正确的架构：

```bash
# 已通过 build_ios.sh 自动生成
xcodebuild -create-xcframework \
  -library ios-libs/libquiche-ios-arm64.a \
  -library ios-libs/libquiche-simulator.a \
  -output libquiche.xcframework
```

### 2. 版本管理

在 git 中添加：
```gitignore
# 忽略编译产物
target/
ios-libs/
*.xcframework/

# 但保留源码
!quiche/chkstk_darwin.c
```

### 3. CI/CD 集成

```yaml
# .github/workflows/ios-build.yml
name: Build iOS

on: [push]

jobs:
  build:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install Rust
        run: |
          curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
          rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios

      - name: Build
        run: ./build_ios.sh

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: libquiche-xcframework
          path: libquiche.xcframework
```

## 📚 相关文档

- [iOS_BUILD_FIX.md](iOS_BUILD_FIX.md) - 详细的编译问题修复指南
- [quiche/include/quiche.h](quiche/include/quiche.h) - C API 文档
- [GitHub Issues](https://github.com/cloudflare/quiche/issues) - 报告问题

## 🔄 更新 quiche

当更新 quiche 版本时：

```bash
# 1. 更新代码
git pull origin master
git submodule update --init --recursive

# 2. 清理旧的编译产物
cargo clean
rm -rf ios-libs/ libquiche.xcframework/

# 3. 重新编译
./build_ios.sh
```

## ⚡ 性能优化

### 减小库大小

已在 Android 优化中应用的技巧也适用于 iOS：

```toml
# .cargo/config.toml
[target.aarch64-apple-ios]
rustflags = [
    "-C", "opt-level=z",        # 最小化大小
    "-C", "codegen-units=1",    # 单一编译单元
    "-C", "panic=abort",        # 移除 panic 展开
    "-C", "link-arg=-Wl,-dead_strip",  # 移除死代码
]
```

### 启用 LTO

```toml
[profile.release]
lto = true
strip = true
```

## 💬 获取帮助

如果遇到问题：

1. 查看 [iOS_BUILD_FIX.md](iOS_BUILD_FIX.md) 获取详细的故障排除指南
2. 检查 [GitHub Issues](https://github.com/cloudflare/quiche/issues)
3. 运行 `./build_ios.sh` 并提供完整的错误日志

---

**最后更新：** 2025-11-04
**quiche 版本：** 0.24.6
**支持的 iOS 版本：** iOS 14.0+
**支持的 Xcode 版本：** Xcode 14.0+（已测试 16.2）
