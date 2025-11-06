# Cargo 构建 Features 问题分析

## 🔍 问题现象

执行以下命令时没有编译 BoringSSL：

```bash
cargo ndk -t armeabi-v7a -- build --features ffi --release
```

---

## 💡 根本原因

### 默认 Features 配置

根据 `quiche/Cargo.toml`，默认 features 包括：

```toml
[features]
default = ["boringssl-vendored", "http3"]
```

### Cargo Features 规则

**关键点：** 当使用 `--features` 时：

| 命令 | 是否包含默认 features | 说明 |
|------|---------------------|------|
| `cargo build` | ✅ **是** | 使用默认 features |
| `cargo build --features ffi` | ✅ **是** | 默认 features + ffi |
| `cargo build --no-default-features` | ❌ **否** | 只使用明确指定的 features |
| `cargo build --no-default-features --features ffi` | ❌ **否** | 只有 ffi |

### 为什么没有编译 BoringSSL？

**可能的原因：**

1. **之前使用过 `--no-default-features`**
   - Cargo 缓存了编译结果
   - 即使后续命令包含默认 features，也可能使用缓存

2. **cargo-ndk 的行为**
   - cargo-ndk 可能在某些情况下会修改 features 行为

3. **增量编译缓存**
   - `target/` 目录中的缓存可能不一致

---

## ✅ 解决方案

### 方案 1：显式指定所有 Features（推荐）

```bash
# 明确指定需要的所有 features
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored
```

**优点：**
- 明确控制启用的 features
- 避免默认 features 的不确定性
- 避免编译不需要的 http3

### 方案 2：清理缓存后重新编译

```bash
# 清理所有缓存
cargo clean

# 使用默认 features + ffi
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --features ffi
```

### 方案 3：使用完整的 Features 列表

```bash
# 包含默认 features
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --features ffi,boringssl-vendored,http3
```

---

## 🎯 推荐的构建命令

### Android 编译（完整命令）

```bash
# 设置 NDK 路径
export ANDROID_NDK_HOME=/Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313

# ARMv7 (32位) - 不包含 HTTP/3
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored

# ARM64 (64位) - 不包含 HTTP/3
cargo ndk -t arm64-v8a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored
```

### 如果需要 HTTP/3

```bash
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,http3
```

---

## 🔍 验证 BoringSSL 是否编译

### 方法 1：检查编译输出

```bash
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored \
  2>&1 | grep -E "(boringssl|cmake)"
```

应该看到类似输出：
```
Compiling cmake v0.1.54
...
running: "cmake" ...
```

### 方法 2：检查 target 目录

```bash
# 检查 BoringSSL 构建产物
find target -name "libssl.a" -o -name "libcrypto.a"
```

### 方法 3：检查最终库的符号

```bash
nm target/armv7-linux-androideabi/release/libquiche.a 2>/dev/null | \
  grep -E "SSL_|CRYPTO_" | head -10
```

如果看到 SSL_* 和 CRYPTO_* 符号，说明 BoringSSL 已链接。

---

## 📊 Features 对比表

| Features 组合 | BoringSSL | HTTP/3 | FFI | 库大小 | 用途 |
|--------------|----------|--------|-----|--------|------|
| `ffi` （默认启用 default） | ✅ | ✅ | ✅ | ~2.2MB | 完整功能 |
| `--no-default-features --features ffi,boringssl-vendored` | ✅ | ❌ | ✅ | ~1.4MB | **推荐 Android** |
| `--no-default-features --features ffi,boringssl-vendored,http3` | ✅ | ✅ | ✅ | ~2.2MB | 完整功能（明确） |
| `--no-default-features --features ffi` | ❌ | ❌ | ✅ | ❌ **编译失败** | 缺少加密库 |

---

## 🐛 常见问题排查

### Q1: 编译时没有看到 cmake 输出

**原因：** 没有启用 `boringssl-vendored` feature

**检查：**
```bash
cargo build --release --features ffi -vv 2>&1 | grep features
```

查看输出中的 features 列表。

**解决：**
```bash
cargo build --release --no-default-features \
  --features ffi,boringssl-vendored
```

### Q2: 链接错误：undefined reference to SSL_*

**原因：** BoringSSL 没有被编译或链接

**检查：**
```bash
# 查看是否有 BoringSSL 静态库
ls -lh target/*/release/build/quiche-*/out/build/
```

**解决：**
```bash
cargo clean
cargo build --release --features ffi,boringssl-vendored
```

### Q3: 为什么之前可以编译，现在不行了？

**原因：** 增量编译缓存问题

**解决：**
```bash
# 完全清理
cargo clean
rm -rf target/

# 重新编译
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features --features ffi,boringssl-vendored
```

### Q4: cargo-ndk 和 cargo build 的区别

**cargo-ndk 实际做什么：**
```bash
# 你运行：
cargo ndk -t armeabi-v7a -- build --features ffi

# 实际执行：
cargo build --target armv7-linux-androideabi --features ffi
# + 设置 Android NDK 环境变量
# + 配置链接器
```

cargo-ndk **不会改变** features 行为。

---

## 📝 最佳实践

### 1. 总是使用 --no-default-features（推荐）

```bash
# ✅ 明确控制
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored

# ❌ 依赖默认行为（不推荐）
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --features ffi
```

### 2. 清理缓存

```bash
# 在切换 features 时
cargo clean

# 或只清理特定目标
cargo clean --target armv7-linux-androideabi
```

### 3. 使用构建脚本

```bash
#!/bin/bash
# build_android.sh

export ANDROID_NDK_HOME=/path/to/ndk

FEATURES="ffi,boringssl-vendored"

for ARCH in armeabi-v7a arm64-v8a x86 x86_64; do
    echo "Building for $ARCH..."
    cargo ndk -t $ARCH -P 21 -- build --release \
        --no-default-features \
        --features $FEATURES
done
```

### 4. 验证 Features

```bash
# 查看实际启用的 features
cargo tree --features ffi,boringssl-vendored \
  -e features --prefix none | grep quiche
```

---

## 🔧 调试命令

### 显示详细编译过程

```bash
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features \
  --features ffi,boringssl-vendored \
  -vv 2>&1 | tee build.log
```

### 检查 build.rs 的条件判断

```bash
# 查看 build.rs 中的 feature 检查
grep -A 5 "cfg!(feature" quiche/src/build.rs
```

输出：
```rust
if cfg!(feature = "boringssl-vendored") &&
    !cfg!(feature = "boringssl-boring-crate") &&
    !cfg!(feature = "openssl")
{
    // 编译 BoringSSL
}
```

### 检查环境变量

```bash
# BoringSSL 路径（可选）
echo $QUICHE_BSSL_PATH

# BoringSSL 链接类型（可选）
echo $QUICHE_BSSL_LINK_KIND
```

---

## 📚 相关文档

- **Cargo Book - Features**: https://doc.rust-lang.org/cargo/reference/features.html
- **cargo-ndk**: https://github.com/bbqsrc/cargo-ndk
- **quiche Features**: `quiche/Cargo.toml`
- **Build Script**: `quiche/src/build.rs`

---

## 🎯 快速参考

### 最常用命令

```bash
# Android ARMv7（推荐）
cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features --features ffi,boringssl-vendored

# Android ARM64（推荐）
cargo ndk -t arm64-v8a -P 21 -- build --release \
  --no-default-features --features ffi,boringssl-vendored

# 清理并重新编译
cargo clean && cargo ndk -t armeabi-v7a -P 21 -- build --release \
  --no-default-features --features ffi,boringssl-vendored
```

### 检查是否包含 BoringSSL

```bash
# 快速检查
nm target/armv7-linux-androideabi/release/libquiche.a | \
  grep SSL_new | head -1

# 有输出 = BoringSSL 已链接
# 无输出 = BoringSSL 未链接
```

---

**总结：** 问题根源是没有明确指定 `boringssl-vendored` feature。虽然它在默认 features 中，但使用 `--no-default-features` 配合明确的 features 列表是最安全的做法。

**最后更新：** 2025-11-05
**quiche 版本：** 0.24.6
