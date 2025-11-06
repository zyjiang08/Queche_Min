# iOS Xcode 16.2 编译修复指南

## 问题描述

在 iOS 平台使用 Xcode 16.2 编译 quiche 时，可能会遇到以下链接错误：

```
Undefined symbols for architecture arm64:
  "___chkstk_darwin", referenced from:
      _CRYPTO_sysrand in libquiche.a(fork_detect.c.o)
      ...
ld: symbol(s) not found for architecture arm64
```

## 问题原因

`__chkstk_darwin` 是一个栈检查函数，在某些编译配置下，LLVM/Clang 会生成对这个函数的调用。但在 iOS 编译环境中，这个符号可能没有被正确链接。

这个问题通常出现在：
- Xcode 16.x 新版本
- BoringSSL 静态库编译
- iOS ARM64/ARM 架构
- 使用了栈探测（stack probing）的代码

## 解决方案

### 方案 1：添加 __chkstk_darwin 实现（推荐）

我们已经创建了 `quiche/chkstk_darwin.c` 文件来提供这个符号的实现。

#### 步骤 1：验证文件存在

```bash
ls -l quiche/chkstk_darwin.c
```

#### 步骤 2：编译并链接

在您的 iOS 项目或 Cargo 配置中，需要将这个 C 文件编译并链接到最终的库中。

**方法 A：使用 cc crate（如果 quiche 有 build.rs）**

在 `quiche/Cargo.toml` 的 `[build-dependencies]` 中添加：
```toml
[build-dependencies]
cc = "1.0"
```

创建 `quiche/build.rs` 文件：
```rust
fn main() {
    // Only compile this for iOS targets
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap();

    if target_os == "ios" {
        cc::Build::new()
            .file("chkstk_darwin.c")
            .compile("chkstk_darwin");

        println!("cargo:rerun-if-changed=chkstk_darwin.c");
    }
}
```

**方法 B：在 Xcode 项目中添加**

1. 将 `quiche/chkstk_darwin.c` 添加到 Xcode 项目中
2. 确保它被包含在编译目标中
3. 重新编译项目

### 方案 2：修改编译器标志（临时方案）

在 `.cargo/config.toml` 中为 iOS 目标添加链接参数：

```toml
[target.aarch64-apple-ios]
rustflags = [
    "-C", "link-arg=-undefined",
    "-C", "link-arg=dynamic_lookup",
]

[target.aarch64-apple-ios-sim]
rustflags = [
    "-C", "link-arg=-undefined",
    "-C", "link-arg=dynamic_lookup",
]

[target.x86_64-apple-ios]
rustflags = [
    "-C", "link-arg=-undefined",
    "-C", "link-arg=dynamic_lookup",
]
```

**注意：** 这个方法允许未定义的符号，只在开发测试时使用，不推荐用于生产环境。

### 方案 3：禁用栈探测（不推荐）

修改 Cargo 编译配置，禁用栈探测：

```toml
[profile.release]
# 其他配置...

[target.aarch64-apple-ios]
rustflags = [
    "-C", "stack-protector=none",
]
```

### 方案 4：使用 boring 而非 boringssl-vendored

如果您的项目允许，可以尝试使用 `boringssl-boring-crate` feature 而不是 `boringssl-vendored`：

```toml
[dependencies]
quiche = { version = "0.24", default-features = false, features = ["ffi", "boringssl-boring-crate"] }
```

这会使用预编译的 BoringSSL crate，可能避免编译问题。

## iOS 编译完整步骤

### 前提条件

```bash
# 安装 Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 添加 iOS 目标
rustup target add aarch64-apple-ios          # iOS 设备 (ARM64)
rustup target add aarch64-apple-ios-sim      # iOS 模拟器 (ARM64)
rustup target add x86_64-apple-ios           # iOS 模拟器 (Intel)

# 安装 cargo-lipo（用于创建通用库）
cargo install cargo-lipo
```

### 编译步骤

#### 1. 修改 Cargo.toml（如果使用方案 1）

在 `quiche/Cargo.toml` 中：

```toml
[build-dependencies]
cc = "1.0"
```

#### 2. 创建 build.rs（方案 1）

```bash
cat > quiche/build.rs << 'EOF'
fn main() {
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();

    if target_os == "ios" {
        cc::Build::new()
            .file("chkstk_darwin.c")
            .compile("chkstk_darwin");

        println!("cargo:rerun-if-changed=chkstk_darwin.c");
    }
}
EOF
```

#### 3. 编译 iOS 库

```bash
# 编译 iOS 设备版本 (ARM64)
cargo build --release \
  --target aarch64-apple-ios \
  --no-default-features \
  --features ffi,boringssl-vendored

# 编译 iOS 模拟器版本 (ARM64)
cargo build --release \
  --target aarch64-apple-ios-sim \
  --no-default-features \
  --features ffi,boringssl-vendored

# 编译 iOS 模拟器版本 (x86_64，如果需要)
cargo build --release \
  --target x86_64-apple-ios \
  --no-default-features \
  --features ffi,boringssl-vendored
```

#### 4. 创建通用库（可选）

如果需要同时支持设备和模拟器：

```bash
# 创建 XCFramework
xcodebuild -create-xcframework \
  -library target/aarch64-apple-ios/release/libquiche.a \
  -library target/aarch64-apple-ios-sim/release/libquiche.a \
  -output libquiche.xcframework
```

或者使用 lipo 合并模拟器库：

```bash
lipo -create \
  target/aarch64-apple-ios-sim/release/libquiche.a \
  target/x86_64-apple-ios/release/libquiche.a \
  -output libquiche-simulator.a
```

## 验证修复

编译成功后，检查符号：

```bash
# 检查是否包含 __chkstk_darwin
nm -g target/aarch64-apple-ios/release/libquiche.a | grep chkstk

# 应该看到类似输出：
# 0000000000000000 T ___chkstk_darwin
```

## Xcode 集成

### 1. 添加库到 Xcode 项目

1. 将生成的 `libquiche.a` 拖到 Xcode 项目中
2. 在 "Build Phases" → "Link Binary With Libraries" 中确认已添加
3. 添加头文件搜索路径到 `quiche/include`

### 2. 配置 Build Settings

在 Xcode 的 Build Settings 中：

- **Other Linker Flags**: 添加 `-lquiche`
- **Library Search Paths**: 添加库文件路径
- **Header Search Paths**: 添加 `quiche/include`

### 3. 添加系统框架

在 "Link Binary With Libraries" 中添加：
- `Security.framework`
- `libresolv.tbd`

## 常见问题

### Q1: 编译时提示 "cmake not found"

```bash
# macOS
brew install cmake

# 验证
cmake --version
```

### Q2: 仍然报 undefined symbol 错误

检查以下几点：
1. 确认 `chkstk_darwin.c` 已被编译
2. 检查 `build.rs` 是否正确执行
3. 清理并重新编译：
   ```bash
   cargo clean
   cargo build --release --target aarch64-apple-ios --features ffi,boringssl-vendored
   ```

### Q3: Xcode 16.2 特定问题

Xcode 16.2 使用了新的链接器，可能需要额外的链接标志：

在 `.cargo/config.toml` 中：
```toml
[target.aarch64-apple-ios]
rustflags = [
    "-C", "link-arg=-Wl,-ld_classic",  # 使用旧版链接器
]
```

### Q4: 模拟器和真机库不兼容

需要分别编译：
- 真机使用 `aarch64-apple-ios`
- 模拟器使用 `aarch64-apple-ios-sim` 或 `x86_64-apple-ios`

不要混用这些架构的库。

## 性能影响

添加 `__chkstk_darwin` 的空实现对性能影响极小：
- 只是一个内存屏障指令
- 在 iOS 上通常不会被频繁调用
- 对运行时性能影响可忽略不计

## 替代方案：使用预编译库

如果编译问题持续存在，考虑以下方案：

1. **使用 CloudFlare 官方预编译库**（如果有提供）
2. **使用 CocoaPods**：
   ```ruby
   # Podfile
   pod 'quiche-ios', :git => 'https://github.com/cloudflare/quiche.git'
   ```
3. **使用 Swift Package Manager**（如果支持）

## 相关链接

- **quiche GitHub**: https://github.com/cloudflare/quiche
- **BoringSSL**: https://boringssl.googlesource.com/boringssl/
- **Rust iOS 编译指南**: https://doc.rust-lang.org/rustc/platform-support/apple-ios.html
- **LLVM Stack Probing**: https://llvm.org/docs/StackMaps.html

## 更新日志

- **2025-11-04**: 创建文档，添加 `chkstk_darwin.c` 实现
- 适用于 Xcode 16.2, quiche 0.24.6, iOS 14.0+

---

如果问题仍然存在，请提供完整的编译错误日志以便进一步诊断。
