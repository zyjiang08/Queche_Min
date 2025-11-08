# libquiche 包大小裁剪优化方案

## 📊 当前状态分析

### 文件大小统计
```
libquiche.a:               44 MB  (562个对象文件)
libquiche_engine.a:        60 MB  (包含 libquiche + libev + 额外封装)
quic-server 二进制:        4.6 MB (链接后)
quic-client 二进制:        4.8 MB (链接后)
```

### 组成分析
- **Rust quiche 核心**: ~16个CGU文件 (代码生成单元)
- **BoringSSL C++代码**: 37个 .cc.o 文件
- **BoringSSL C代码**: 231个 .c.o 文件  
- **BoringSSL 汇编**: 18个 .S.o 文件
- **Rust 标准库依赖**: ~260个文件 (std, alloc, core, etc.)

## 🔍 代码实际使用情况

### Server (server.c) 使用的API
```c
// 配置相关 (8个)
quiche_config_new()
quiche_config_load_cert_chain_from_pem_file()
quiche_config_load_priv_key_from_pem_file()
quiche_config_set_application_protos()
quiche_config_set_initial_max_data()
quiche_config_set_initial_max_stream_data_*()
quiche_config_set_max_idle_timeout()
quiche_config_set_cc_algorithm()

// 连接管理 (7个)
quiche_accept()
quiche_negotiate_version()
quiche_retry()
quiche_version_is_supported()
quiche_conn_recv()
quiche_conn_send()
quiche_conn_on_timeout()

// 流操作 (4个)
quiche_conn_stream_send()
quiche_conn_stream_recv()
quiche_stream_iter_next()
quiche_conn_readable()

// 统计信息 (3个)
quiche_conn_stats()
quiche_conn_path_stats()
quiche_conn_is_established()
quiche_conn_is_closed()
```

### Client (client.cpp) 使用的功能
- **仅使用 quiche_engine 封装**
- 不直接调用 quiche C API
- 需要完整的 libquiche_engine.a

### ❌ 未使用的功能（可裁剪）

#### 1. HTTP/3 相关 (~8-10MB)
```rust
- quiche::h3::*           // HTTP/3 实现
- quiche::qpack::*        // QPACK 头部压缩
- huffman 编解码
- 动态表管理
```
**影响**: 本项目仅使用 QUIC 传输层，不使用 HTTP/3

#### 2. DTLS 相关功能 (~2-3MB)
```
- d1_both.cc.o           // DTLS 双向通信
- d1_lib.cc.o            // DTLS 库函数
- d1_pkt.cc.o            // DTLS 数据包
- d1_srtp.cc.o           // DTLS-SRTP
- dtls_method.cc.o
- dtls_record.cc.o
```
**影响**: QUIC 不使用 DTLS，仅使用 TLS 1.3

#### 3. 未使用的加密算法 (~5-7MB)
```
- RC2, RC4 (过时算法)
- MD5 (不安全)
- DSA (已废弃)
- DH (可选)
- PKCS7, PKCS8 (部分功能)
- HRSS (后量子密码，可选)
```
**影响**: QUIC 仅需要现代 TLS 1.3 密码套件

#### 4. 多平台支持代码 (~1-2MB)
```
- cpu-aarch64-fuchsia.c.o
- cpu-aarch64-win.c.o  
- cpu-arm-linux.c.o (如果目标是 x86_64)
- cpu-ppc64le.c.o
- windows.c.o (macOS/Linux 不需要)
- fuchsia.c.o
```
**影响**: 每个平台只需要对应平台的代码

#### 5. 调试和符号信息 (~3-5MB)
```
- addr2line, gimli (栈回溯)
- rustc_demangle (符号解析)
- panic_unwind (展开)
```
**影响**: Release 构建可裁剪

#### 6. 未使用的 QUIC 功能 (~2-3MB)
```
- 连接迁移 (已禁用)
- 0-RTT (未使用)  
- Datagram 扩展 (未使用)
- 多路径 QUIC (未使用)
```

## 🎯 裁剪方案

### 方案一：编译时特性裁剪（推荐）⭐

修改 quiche 构建配置：

```toml
# Cargo.toml 或构建命令
[features]
default = []  # 移除默认特性

# 仅启用必要特性
minimal = [
    "boringssl-vendored",  # TLS 支持
    "ffi",                 # C FFI
]

# 禁用的特性
# qlog = []              # 日志格式
# sfv = []               # 结构化字段值  
```

**构建命令**:
```bash
# 最小化构建
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored \
  --target x86_64-apple-darwin

# 预期减小: 15-20MB (44MB → 24-29MB)
```

### 方案二：BoringSSL 裁剪

修改 BoringSSL 构建配置 (`quiche/deps/boringssl/`):

```cmake
# CMakeLists.txt 添加
set(OPENSSL_NO_RC2 ON)
set(OPENSSL_NO_RC4 ON)  
set(OPENSSL_NO_MD5 ON)
set(OPENSSL_NO_DSA ON)
set(OPENSSL_NO_DTLS ON)
set(OPENSSL_NO_SRTP ON)
```

**预期减小**: 5-8MB

### 方案三：Strip 符号表

```bash
# 构建后处理
strip -S lib/libquiche.a          # 保留全局符号
strip -x lib/libquiche.a          # 保留外部符号
strip lib/libquiche.a              # 全部移除（不推荐）

# 二进制处理
strip -S quic-server
strip -S quic-client-macos-x86_64

# 预期减小: 30-40%
```

### 方案四：链接时优化 (LTO)

```bash
# 启用 LTO 和优化
cargo build --release \
  --features ffi,boringssl-vendored \
  --no-default-features
  
# 在 Cargo.toml 添加
[profile.release]
lto = true              # 链接时优化
codegen-units = 1       # 单个代码生成单元
opt-level = "z"         # 优化大小
strip = true            # 自动 strip
panic = "abort"         # 移除 unwinding
```

**预期减小**: 20-30%

### 方案五：目标平台专用构建

```bash
# 仅构建 x86_64 macOS
cargo build --release \
  --target x86_64-apple-darwin \
  --features ffi,boringssl-vendored \
  --no-default-features

# 移除未使用平台代码
# 预期减小: 2-3MB
```

## 📋 完整裁剪流程

### 第一阶段：立即可行（无需修改源码）

```bash
# 1. 使用最小特性构建
cd quiche
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored

# 2. Strip 符号
strip -S target/release/libquiche.a

# 3. 复制到 quic-demo
cp target/release/libquiche.a quiche/quic-demo/lib/

# 预期: 44MB → 25-30MB (节省 30-40%)
```

### 第二阶段：配置优化

```toml
# quiche/Cargo.toml
[profile.release]
lto = true
codegen-units = 1  
opt-level = "z"
strip = true
panic = "abort"

[profile.release.package.ring]
opt-level = 3  # ring 需要速度

[profile.release.package.boringssl-sys]
opt-level = "z"
```

重新构建：
```bash
cargo clean
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored

# 预期: 25-30MB → 18-22MB (再减 25-30%)
```

### 第三阶段：深度裁剪（需要修改源码）

1. **移除 HTTP/3 模块**
   ```toml
   # 禁用 h3 feature
   # 移除 src/h3/ 目录编译
   ```

2. **BoringSSL 精简**
   - 修改 `deps/boringssl/CMakeLists.txt`
   - 禁用 DTLS, RC4, MD5 等

3. **Rust 依赖裁剪**
   ```toml
   [dependencies]
   # 移除未使用的 crates
   # 使用 no_std 变体
   ```

**预期总计减小**: 44MB → 12-15MB (减少 65-70%)

## 🧪 验证方法

```bash
# 检查库大小
ls -lh lib/libquiche.a

# 检查二进制大小
ls -lh quic-server quic-client-*

# 验证功能
./quic-server 127.0.0.1 4433 &
./quic-client-macos-x86_64 127.0.0.1 4433

# 检查未使用符号
nm -u quic-server | wc -l  # 未定义符号数量
nm -g lib/libquiche.a | grep " T " | wc -l  # 导出符号数量
```

## ⚠️ 注意事项

1. **兼容性风险**
   - 裁剪后可能影响未来功能扩展
   - 建议保留完整版本作为备份

2. **测试要求**
   - 每次裁剪后必须完整测试
   - 检查所有代码路径

3. **维护成本**
   - 定制构建需要维护 build 脚本
   - quiche 更新时需要重新适配

4. **性能影响**
   - `opt-level="z"` 可能略微降低性能
   - LTO 编译时间显著增加

## 📊 预期效果对比

| 方案 | 大小 | 减少 | 难度 | 风险 |
|------|------|------|------|------|
| 原始 | 44MB | - | - | - |
| 方案一 (特性) | 29MB | 34% | 低 | 低 |
| 方案二 (BoringSSL) | 36MB | 18% | 中 | 中 |
| 方案三 (Strip) | 26MB | 41% | 低 | 低 |
| 方案四 (LTO) | 31MB | 30% | 低 | 低 |
| 组合 (1+3+4) | 18MB | 59% | 中 | 中 |
| 深度裁剪 | 12-15MB | 66-72% | 高 | 高 |

## 🚀 快速实施建议

**立即可行**（5分钟）:
```bash
cd quiche
cargo build --release --no-default-features --features ffi,boringssl-vendored
strip -S target/release/libquiche.a
cp target/release/libquiche.a quiche/quic-demo/lib/
cd quiche/quic-demo && make clean && make
```

**效果**: 44MB → ~26MB，减少 40%，无风险。
