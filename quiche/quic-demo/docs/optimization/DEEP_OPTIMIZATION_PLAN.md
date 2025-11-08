# libquiche/libquiche_engine 深度优化方案

## 📊 当前状态

### 已完成的优化（第一阶段）
```
libquiche.a:     44 MB → 15 MB  (-66%, 已实施)
quic-server:     4.6 MB → 3.9 MB (-15%, 已实施)
quic-client:     4.6 MB (待优化)
```

### 待优化的主要方向

1. ✅ **已移除**: HTTP/3 实现 (~8-10MB)
2. ❌ **仍存在**: DTLS支持 (6个文件, ~2-3MB)
3. ❌ **仍存在**: 未使用的加密算法 (RC2, RC4, MD5, DSA等, ~5-7MB)
4. ❌ **仍存在**: 多平台支持代码 (~1-2MB)
5. ❌ **未实施**: 符号可见性控制（暴露所有符号）
6. ❌ **未实施**: LTO链接时优化
7. ❌ **未实施**: 平台专用裁剪

## 🎯 深度优化目标

| 库 | 当前大小 | 目标大小 | 减小幅度 |
|---|---------|---------|---------|
| **libquiche.a** | 15 MB | 8-10 MB | **-47%** |
| **libquiche_engine.a** | 60 MB | 12-15 MB | **-75%** |
| **quic-client** | 4.6 MB | 2.5-3.0 MB | **-43%** |

## 📋 分步骤实施计划

---

## 步骤 1: 符号可见性控制

### 1.1 目标
只暴露 QuicheEngine 的公开 API，隐藏所有内部实现符号。

### 1.2 分析当前暴露的符号

```bash
# 检查 libquiche_engine.a 导出的符号
nm -gU lib/macos/x86_64/libquiche_engine.a | grep " T " | wc -l

# 检查 quic-client 使用的 QuicheEngine 符号
nm -gU quic-client | grep -E "QuicheEngine|quiche_engine" | c++filt
```

### 1.3 需要暴露的符号（仅QuicheEngine API）

根据 `engine/include/quiche_engine.h` 分析，仅需暴露：

```cpp
// QuicheEngine 类的公开方法
class QuicheEngine {
public:
    QuicheEngine(const std::string& server, ...);
    ~QuicheEngine();

    void start();
    void shutdown(...);
    ssize_t write(...);
    ssize_t read(...);
    void setEventCallback(...);
    EngineStats getStats();
};

// 相关的枚举和结构体
enum class ConfigKey { ... };
enum class EngineEvent { ... };
struct EngineStats { ... };
struct EventData { ... };
```

### 1.4 实施方法

#### 方法 A: 使用 visibility attributes (推荐)

修改 `engine/include/quiche_engine.h`:

```cpp
// 添加可见性宏
#if defined(__GNUC__) || defined(__clang__)
  #define QUICHE_ENGINE_EXPORT __attribute__((visibility("default")))
  #define QUICHE_ENGINE_LOCAL  __attribute__((visibility("hidden")))
#else
  #define QUICHE_ENGINE_EXPORT
  #define QUICHE_ENGINE_LOCAL
#endif

// 标记公开API
class QUICHE_ENGINE_EXPORT QuicheEngine {
    // ...
};

// 隐藏内部实现
class QUICHE_ENGINE_LOCAL QuicheEngineImpl {
    // ...
};
```

#### 方法 B: 使用 linker version script

创建 `engine/exports.txt`:

```
# QuicheEngine exports
_ZN6quiche12QuicheEngine*
_ZN6quiche10ConfigKey*
_ZN6quiche11EngineEvent*
_ZN6quiche11EngineStats*
_ZN6quiche9EventData*
```

修改链接参数:

```bash
# macOS
-Wl,-exported_symbols_list,exports.txt

# Linux
-Wl,--version-script=exports.txt
```

### 1.5 构建配置

修改 `engine/Makefile`:

```makefile
# 添加符号可见性控制
CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

# 仅导出必要符号
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -Wl,-exported_symbols_list,$(SRC_DIR)/exports_macos.txt
else ifeq ($(UNAME_S),Linux)
    LDFLAGS += -Wl,--version-script=$(SRC_DIR)/exports_linux.txt
endif
```

### 1.6 验证

```bash
# 重新构建
cd engine
make clean && make PLATFORM=macos ARCH=x86_64

# 检查导出符号数量（应该大幅减少）
nm -gU lib/macos/x86_64/libquiche_engine.a | grep " T " | wc -l

# 检查库大小
ls -lh lib/macos/x86_64/libquiche_engine.a

# 验证功能
cd ../quic-demo
make clean && make
./test_communication.sh
```

**预期效果**:
- 导出符号: ~1000+ → ~50
- 库大小: 无显著变化（为步骤2铺垫）

---

## 步骤 2: 链接时优化 (LTO) 和死代码消除

### 2.1 目标
启用 LTO 和 dead code elimination，移除所有未使用的函数和变量。

### 2.2 配置 Cargo.toml

修改 `quiche/Cargo.toml`:

```toml
[profile.release]
lto = "fat"              # 完整 LTO（而非 "thin"）
codegen-units = 1        # 单个代码生成单元（牺牲编译速度换体积）
opt-level = "z"          # 优化大小（从 "3" 改为 "z"）
strip = true             # 自动 strip 符号
panic = "abort"          # 移除 panic unwinding
overflow-checks = false  # 禁用溢出检查（Release）

# BoringSSL 专用优化
[profile.release.package.boringssl-sys]
opt-level = "z"          # BoringSSL 也优化大小

# 保持 Ring 性能（如果使用）
[profile.release.package.ring]
opt-level = 3            # Ring 需要速度
```

### 2.3 配置 C/C++ 编译器

修改 `engine/Makefile`:

```makefile
# LTO 编译标志
CFLAGS += -flto=full -ffunction-sections -fdata-sections
CXXFLAGS += -flto=full -ffunction-sections -fdata-sections

# LTO 链接标志
LDFLAGS += -flto=full -Wl,-dead_strip  # macOS
# LDFLAGS += -flto=full -Wl,--gc-sections  # Linux
```

### 2.4 修改 BoringSSL 构建

修改 `quiche/src/build.rs` 中的 BoringSSL 构建：

```rust
// 在 cmake 配置中添加
.define("CMAKE_C_FLAGS_RELEASE", "-flto=full -ffunction-sections")
.define("CMAKE_CXX_FLAGS_RELEASE", "-flto=full -ffunction-sections")
.define("CMAKE_EXE_LINKER_FLAGS", "-flto=full -Wl,-dead_strip")
```

### 2.5 构建命令

```bash
# 清理旧构建
cd /path/to/Queche_Min
cargo clean

# 重新构建 libquiche（启用LTO）
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 重新构建 libquiche_engine
./quiche_engine_all.sh macos x86_64

# 检查效果
ls -lh target/release/libquiche.a
ls -lh lib/macos/x86_64/libquiche_engine.a
```

### 2.6 验证

```bash
# 检查链接的函数数量
nm -gU lib/macos/x86_64/libquiche_engine.a | grep " T " | wc -l

# 构建 client 并检查大小
cd quiche/quic-demo
make clean && make
ls -lh quic-client

# 功能测试
./test_communication.sh
```

**预期效果**:
- libquiche.a: 15 MB → 10-12 MB (-25-30%)
- libquiche_engine.a: 60 MB → 18-22 MB (-63-65%)
- quic-client: 4.6 MB → 3.2-3.5 MB (-25-30%)

---

## 步骤 3: BoringSSL 深度裁剪

### 3.1 分析 QUIC 实际使用的 BoringSSL 功能

```bash
# 提取 quiche 源码中使用的 SSL/EVP 函数
cd quiche/src
grep -rh "SSL_\|EVP_\|CRYPTO_" *.rs | \
  sed 's/.*\(SSL_[a-zA-Z0-9_]*\).*/\1/' | \
  sort -u > /tmp/used_ssl_funcs.txt

# 检查结果
cat /tmp/used_ssl_funcs.txt | head -20
```

### 3.2 QUIC 需要的 BoringSSL 模块

根据 QUIC/TLS 1.3 规范，仅需要：

**TLS 1.3 必需**:
- ✅ TLS 1.3 handshake (tls13_*.cc)
- ✅ X.509 证书验证 (x509*.c)
- ✅ 现代密码套件:
  - AES-128-GCM
  - AES-256-GCM
  - ChaCha20-Poly1305
- ✅ ECDHE key exchange (P-256, X25519)
- ✅ HKDF (密钥派生)

**QUIC 不需要（可裁剪）**:
- ❌ DTLS (d1_*.cc, dtls_*.cc) - 6个文件
- ❌ SSL 3.0/TLS 1.0/1.1/1.2 (ssl3_*.cc)
- ❌ 过时加密算法:
  - RC2, RC4 (e_rc2.c, e_rc4.c, rc4.c)
  - MD5 (md5*.c)
  - DSA (dsa*.c)
  - DH (非ECDH) (dh*.c)
- ❌ SRTP (d1_srtp.cc)
- ❌ 未使用的曲线 (除 P-256, X25519 外)
- ❌ 后量子密码 HRSS (hrss.c) - 如果不需要

### 3.3 创建 BoringSSL 裁剪配置

创建 `quiche/boringssl_trim_config.cmake`:

```cmake
# BoringSSL 裁剪配置

# 禁用 DTLS
add_definitions(-DOPENSSL_NO_DTLS)
add_definitions(-DOPENSSL_NO_DTLS1)

# 禁用过时协议
add_definitions(-DOPENSSL_NO_SSL3)
add_definitions(-DOPENSSL_NO_TLS1)
add_definitions(-DOPENSSL_NO_TLS1_1)

# 禁用过时算法
add_definitions(-DOPENSSL_NO_RC2)
add_definitions(-DOPENSSL_NO_RC4)
add_definitions(-DOPENSSL_NO_MD5)
add_definitions(-DOPENSSL_NO_DSA)
add_definitions(-DOPENSSL_NO_SRTP)

# 禁用后量子密码（如果不需要）
add_definitions(-DOPENSSL_NO_HRSS)

# 仅保留必要的曲线
add_definitions(-DOPENSSL_CURVES=P-256:X25519)
```

### 3.4 修改 build.rs

修改 `quiche/src/build.rs`:

```rust
fn build_boringssl() {
    let mut cfg = cmake::Config::new("deps/boringssl");

    // ... 现有配置 ...

    // 添加裁剪配置
    cfg.define("CMAKE_C_FLAGS",
        "-DOPENSSL_NO_DTLS \
         -DOPENSSL_NO_RC2 \
         -DOPENSSL_NO_RC4 \
         -DOPENSSL_NO_MD5 \
         -DOPENSSL_NO_DSA \
         -DOPENSSL_NO_SRTP");

    cfg.define("CMAKE_CXX_FLAGS",
        "-DOPENSSL_NO_DTLS \
         -DOPENSSL_NO_SRTP");

    // ... 继续构建 ...
}
```

### 3.5 手动移除不需要的 BoringSSL 源文件

创建脚本 `quiche/trim_boringssl.sh`:

```bash
#!/bin/bash

BSSL_DIR="deps/boringssl"

# 备份
cp -r $BSSL_DIR ${BSSL_DIR}.backup

# 移除 DTLS 相关文件
rm -f $BSSL_DIR/ssl/d1_*.cc
rm -f $BSSL_DIR/ssl/dtls_*.cc

# 移除过时算法
rm -f $BSSL_DIR/crypto/rc2/*
rm -f $BSSL_DIR/crypto/rc4/*
rm -f $BSSL_DIR/crypto/dsa/*
rm -f $BSSL_DIR/crypto/md5/*

# 移除 SRTP
find $BSSL_DIR -name "*srtp*" -delete

# 移除 HRSS (后量子密码)
rm -f $BSSL_DIR/crypto/hrss/*

echo "BoringSSL 裁剪完成"
echo "原始文件备份在: ${BSSL_DIR}.backup"
```

### 3.6 构建和验证

```bash
# 裁剪 BoringSSL
cd quiche
./trim_boringssl.sh

# 重新构建
cargo clean
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 检查大小
ls -lh target/release/libquiche.a

# 验证功能（应该仍然正常工作）
cd quic-demo
make clean && make
./test_communication.sh
```

**预期效果**:
- libquiche.a: 10-12 MB → 6-8 MB (-40-50%)
- 移除文件数: ~30个 BoringSSL 文件

---

## 步骤 4: 精确的功能裁剪

### 4.1 保留 qlog，移除 datagram-socket

修改 `quiche/Cargo.toml`:

```toml
[dependencies]
# 移除 datagram-socket 依赖（如果存在）
# datagram-socket = { ... }  # 注释掉

# 确保 qlog 是可选的
qlog = { workspace = true, optional = true }
```

### 4.2 构建特性配置

```bash
# 构建命令（包含 qlog，排除其他）
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog
```

### 4.3 验证

```bash
# 检查是否包含 qlog
nm target/release/libquiche.a | grep qlog | head -5

# 检查是否移除 datagram
nm target/release/libquiche.a | grep datagram || echo "datagram removed"

# 检查大小
ls -lh target/release/libquiche.a
```

---

## 步骤 5: 平台专用裁剪

### 5.1 共用裁剪（所有平台）

已在前面步骤完成：
- ✅ 移除 HTTP/3
- ✅ 移除 DTLS
- ✅ 移除过时加密算法
- ✅ LTO 优化

### 5.2 iOS 专用裁剪

创建 `quiche_engine_ios.sh`:

```bash
#!/bin/bash

PLATFORM="ios"
ARCH="$1"  # arm64 或 x86_64 (simulator)

# iOS 特定优化
export CFLAGS="\
  -flto=full \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -Os \
  -DOPENSSL_NO_ASM"  # iOS 禁用汇编优化

export CXXFLAGS="$CFLAGS"

# iOS 不需要的 CPU 代码
export TRIM_CPUS="aarch64-fuchsia,aarch64-win,arm-linux,ppc64le,windows,fuchsia"

# 构建
cargo build --release \
  --target ${ARCH}-apple-ios \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 移除未使用 CPU 文件
ar -d target/${ARCH}-apple-ios/release/libquiche.a \
  cpu-aarch64-fuchsia.c.o \
  cpu-aarch64-win.c.o \
  cpu-arm-linux.c.o \
  cpu-ppc64le.c.o \
  windows.c.o \
  fuchsia.c.o

# Strip
strip -S target/${ARCH}-apple-ios/release/libquiche.a
```

**iOS 专用裁剪**:
- ❌ 移除非 ARM64 汇编代码
- ❌ 移除 Windows/Linux 特定代码
- ❌ 禁用部分汇编优化（兼容性）

### 5.3 Android 专用裁剪

创建 `quiche_engine_android.sh`:

```bash
#!/bin/bash

ARCH="$1"  # arm64-v8a, armeabi-v7a, x86, x86_64

# Android 特定优化
export CFLAGS="\
  -flto=full \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -Os"

export CXXFLAGS="$CFLAGS"

# Android 目标
case $ARCH in
  arm64-v8a)
    TARGET="aarch64-linux-android"
    ;;
  armeabi-v7a)
    TARGET="armv7-linux-androideabi"
    ;;
  x86)
    TARGET="i686-linux-android"
    ;;
  x86_64)
    TARGET="x86_64-linux-android"
    ;;
esac

# 构建
cargo build --release \
  --target $TARGET \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 移除未使用 CPU 文件
ar -d target/$TARGET/release/libquiche.a \
  cpu-aarch64-fuchsia.c.o \
  cpu-aarch64-win.c.o \
  cpu-ppc64le.c.o \
  windows.c.o \
  fuchsia.c.o

# Strip
${NDK_BIN}/${TARGET}-strip -S target/$TARGET/release/libquiche.a
```

**Android 专用裁剪**:
- ❌ 移除非目标架构汇编代码
- ❌ 移除 iOS/macOS 特定代码
- ❌ 移除 Windows/Fuchsia 代码

### 5.4 macOS 专用裁剪

创建 `quiche_engine_macos.sh`:

```bash
#!/bin/bash

ARCH="$1"  # x86_64 或 arm64

# macOS 特定优化
export CFLAGS="\
  -flto=full \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -Oz"  # macOS 使用 -Oz

export CXXFLAGS="$CFLAGS"

# 构建
cargo build --release \
  --target ${ARCH}-apple-darwin \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 移除未使用 CPU 文件
ar -d target/${ARCH}-apple-darwin/release/libquiche.a \
  cpu-aarch64-fuchsia.c.o \
  cpu-aarch64-win.c.o \
  cpu-arm-linux.c.o \
  cpu-ppc64le.c.o \
  windows.c.o \
  fuchsia.c.o \
  thread_win.c.o

# Strip (保留必要符号)
strip -S target/${ARCH}-apple-darwin/release/libquiche.a
```

**macOS 专用裁剪**:
- ❌ 移除 ARM-Linux 代码
- ❌ 移除 Windows 线程代码
- ❌ 移除 Fuchsia 代码
- ✅ 保留 x86_64/ARM64 汇编优化

### 5.5 Linux 专用裁剪

```bash
#!/bin/bash

ARCH="$1"  # x86_64 或 arm64

# Linux 特定优化
export CFLAGS="\
  -flto=full \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -Os"

export CXXFLAGS="$CFLAGS"

# 构建
cargo build --release \
  --target ${ARCH}-unknown-linux-gnu \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 移除未使用 CPU 文件
ar -d target/${ARCH}-unknown-linux-gnu/release/libquiche.a \
  cpu-aarch64-fuchsia.c.o \
  cpu-aarch64-win.c.o \
  cpu-ppc64le.c.o \
  windows.c.o \
  fuchsia.c.o \
  thread_win.c.o

# Strip
strip -S target/${ARCH}-unknown-linux-gnu/release/libquiche.a
```

**Linux 专用裁剪**:
- ❌ 移除 Windows/Fuchsia 代码
- ✅ 保留 POSIX 线程代码

### 5.6 平台裁剪对比

| 平台 | 移除的文件 | 额外减小 |
|------|-----------|---------|
| iOS | ARM-Linux, Windows, Fuchsia, PPC | ~0.5-1 MB |
| Android | iOS, macOS, Windows, Fuchsia | ~0.5-1 MB |
| macOS | ARM-Linux, Windows, Fuchsia | ~0.5-1 MB |
| Linux | iOS, macOS, Windows, Fuchsia | ~0.5-1 MB |

---

## 🧪 完整验证流程

### 验证步骤 1: 构建所有优化版本

```bash
# 1. 清理
cd /path/to/Queche_Min
cargo clean
rm -rf lib/

# 2. 构建 macOS 版本（带所有优化）
./quiche_engine_macos.sh x86_64

# 3. 检查库大小
ls -lh target/x86_64-apple-darwin/release/libquiche.a
ls -lh lib/macos/x86_64/libquiche_engine.a

# 4. 构建 quic-client
cd quiche/quic-demo
make clean && make PLATFORM=macos ARCH=x86_64

# 5. 检查 client 大小
ls -lh quic-client
size -m quic-client
```

### 验证步骤 2: 功能测试

```bash
# 连接测试
./test_communication.sh

# 压力测试
for i in {1..10}; do
  echo "Test round $i"
  ./test_communication.sh || break
done

# 长连接测试
timeout 60s ./quic-server 127.0.0.1 4433 &
sleep 2
timeout 50s ./quic-client 127.0.0.1 4433
```

### 验证步骤 3: 符号检查

```bash
# 检查导出符号（应该很少）
nm -gU lib/macos/x86_64/libquiche_engine.a | grep " T " | wc -l

# 检查是否包含 qlog
nm lib/macos/x86_64/libquiche_engine.a | grep qlog || echo "qlog missing!"

# 检查是否移除 DTLS
nm lib/macos/x86_64/libquiche_engine.a | grep dtls && echo "DTLS still present!" || echo "DTLS removed"

# 检查是否移除 RC4
nm lib/macos/x86_64/libquiche_engine.a | grep RC4 && echo "RC4 still present!" || echo "RC4 removed"
```

### 验证步骤 4: 性能测试

```bash
# 吞吐量测试
dd if=/dev/zero bs=1M count=10 | ./quic-client 127.0.0.1 4433

# 延迟测试
ping -c 100 127.0.0.1  # 基准
# 运行 QUIC 测试并比较延迟
```

---

## 📊 预期优化效果总结

### 逐步优化效果

| 步骤 | libquiche.a | libquiche_engine.a | quic-client |
|------|-------------|-------------------|-------------|
| **基线** | 44 MB | 60 MB | 4.6 MB |
| 步骤0 (已完成) | 15 MB (-66%) | - | - |
| 步骤1 (符号) | 15 MB | 60 MB → 58 MB | 4.6 MB |
| 步骤2 (LTO) | 10 MB (-33%) | 58 MB → 20 MB (-65%) | 4.6 MB → 3.2 MB |
| 步骤3 (BoringSSL) | 7 MB (-30%) | 20 MB → 14 MB (-30%) | 3.2 MB → 2.8 MB |
| 步骤4 (功能) | 6.5 MB (-7%) | 13.5 MB (-4%) | 2.7 MB (-4%) |
| 步骤5 (平台) | 6 MB (-8%) | 13 MB (-4%) | 2.6 MB (-4%) |
| **最终** | **6 MB (-86%)** | **13 MB (-78%)** | **2.6 MB (-43%)** |

### 总计节省

- **libquiche.a**: 44 MB → 6 MB (节省 **38 MB, -86%**)
- **libquiche_engine.a**: 60 MB → 13 MB (节省 **47 MB, -78%**)
- **quic-client**: 4.6 MB → 2.6 MB (节省 **2.0 MB, -43%**)

---

## 🚀 自动化脚本

### 一键优化脚本

创建 `optimize_all.sh`:

```bash
#!/bin/bash

set -e

PLATFORM=${1:-macos}
ARCH=${2:-x86_64}

echo "🚀 开始深度优化..."
echo "平台: $PLATFORM, 架构: $ARCH"

# 步骤 0: 清理
echo "步骤 0: 清理旧构建..."
cargo clean
rm -rf lib/$PLATFORM/$ARCH

# 步骤 1: 符号可见性 (通过 Makefile 控制)
echo "步骤 1: 符号可见性控制..."
export CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden"

# 步骤 2: LTO
echo "步骤 2: 启用 LTO..."
export CARGO_PROFILE_RELEASE_LTO=fat
export CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1
export CARGO_PROFILE_RELEASE_OPT_LEVEL=z

# 步骤 3: BoringSSL 裁剪
echo "步骤 3: 裁剪 BoringSSL..."
./trim_boringssl.sh

# 步骤 4 & 5: 构建
echo "步骤 4-5: 构建优化版本..."
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog \
  --target ${ARCH}-apple-${PLATFORM}

# Strip
echo "Strip 符号表..."
strip -S target/${ARCH}-apple-${PLATFORM}/release/libquiche.a

# 构建 engine
echo "构建 libquiche_engine..."
./quiche_engine_${PLATFORM}.sh $ARCH

# 验证
echo "✅ 优化完成！"
ls -lh target/${ARCH}-apple-${PLATFORM}/release/libquiche.a
ls -lh lib/$PLATFORM/$ARCH/libquiche_engine.a

# 构建 client
cd quiche/quic-demo
make clean && make PLATFORM=$PLATFORM ARCH=$ARCH
ls -lh quic-client-${PLATFORM}-${ARCH}

echo "🎉 所有优化步骤完成！"
```

---

## ⚠️ 注意事项

### 风险评估

1. **LTO 编译时间**: 增加 3-5 倍
2. **BoringSSL 裁剪**: 确保不影响 TLS 1.3 握手
3. **符号可见性**: 可能影响调试（保留未优化版本）
4. **平台兼容性**: 每个平台需要独立测试

### 回滚方案

```bash
# 恢复 BoringSSL
cd quiche
rm -rf deps/boringssl
mv deps/boringssl.backup deps/boringssl

# 恢复构建配置
git checkout Cargo.toml engine/Makefile
```

### 维护建议

1. 保留两个构建配置：
   - `release-debug`: 未优化，用于调试
   - `release-optimized`: 完全优化，用于发布

2. 创建 CI/CD 流程，自动测试所有平台

3. 记录每次优化的效果和问题

---

## 📝 检查清单

完成每个步骤后，勾选：

- [ ] 步骤1: 符号可见性控制
  - [ ] 修改头文件
  - [ ] 修改 Makefile
  - [ ] 验证导出符号数量
  - [ ] 功能测试通过

- [ ] 步骤2: LTO 和死代码消除
  - [ ] 修改 Cargo.toml
  - [ ] 修改 Makefile
  - [ ] 验证库大小减小
  - [ ] 功能测试通过

- [ ] 步骤3: BoringSSL 深度裁剪
  - [ ] 备份 BoringSSL
  - [ ] 执行裁剪脚本
  - [ ] 验证库大小减小
  - [ ] TLS 握手测试通过

- [ ] 步骤4: 功能裁剪
  - [ ] 确认 qlog 保留
  - [ ] 确认 datagram 移除
  - [ ] 功能测试通过

- [ ] 步骤5: 平台专用裁剪
  - [ ] iOS 构建测试
  - [ ] Android 构建测试
  - [ ] macOS 构建测试
  - [ ] Linux 构建测试

- [ ] 最终验证
  - [ ] 所有平台功能测试
  - [ ] 性能基准测试
  - [ ] 符号表检查
  - [ ] 文档更新

---

**创建日期**: 2025-11-08
**方案版本**: v2.0 (深度优化版)
**预期完成时间**: 2-3天（包括测试）
