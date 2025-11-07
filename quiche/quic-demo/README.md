# QUIC Demo - 双向数据传输示例

基于 Cloudflare quiche 的 QUIC 协议客户端/服务器演示程序，展示双向数据传输能力。

## 📋 项目概述

本项目包含一个 QUIC 服务器和客户端示例，演示：
- **服务器**：使用 C 语言 + libev + quiche C API
- **客户端**：使用 C++ + quiche_engine 封装
- **双向数据传输**：客户端上传 1MB，服务器下载 7.5MB
- **实时统计信息**：数据包、字节数、RTT、拥塞窗口等

## 📁 目录结构

```
quic-demo/
├── src/
│   ├── server.c          # QUIC 服务器（C + libev）
│   └── client.cpp        # QUIC 客户端（C++ + quiche_engine）
├── include/              # 本地头文件
│   ├── quiche.h          # quiche C API
│   └── uthash.h          # 哈希表实现
├── lib/
│   └── libquiche.a       # quiche 静态库 (~43MB)
├── certs/
│   ├── cert.crt          # 自签名证书（测试用）
│   └── cert.key          # 私钥
├── Makefile             # 便捷构建脚本
├── Makefile.server      # 服务器独立构建（仅 Linux/macOS）
├── Makefile.client      # 客户端跨平台构建
├── build_client.sh      # 客户端多平台自动构建脚本 ⭐
├── build/               # 构建产物目录
├── quic-server          # 服务器二进制（生成）
├── quic-client-*        # 客户端二进制（生成）
└── README.md           # 本文档
```

## 🏗️ 架构说明

### 依赖关系

```
服务器 (server.c)
  ├── libquiche.a    (./lib/)
  ├── libev          (系统库)
  └── 系统库 (pthread, dl, m)

客户端 (client.cpp)
  ├── libquiche_engine.a  (../../lib/<platform>/<arch>/)
  ├── libquiche.a         (./lib/)
  └── 系统库 (pthread, dl, m)
```

### 数据传输模式

- **客户端 → 服务器**：200KB/秒 × 5秒 = 1MB
- **服务器 → 客户端**：1.5MB/秒 × 5秒 = 7.5MB
- **客户端模式**：轮询接收（polling），等待时间 8 秒
- **统计输出**：完整的连接统计和应用层数据统计

## 🔧 前置条件

### 1. 构建 quiche_engine 库

客户端需要预先构建的 `libquiche_engine.a`：

```bash
# 从项目根目录
cd ../..

# 构建当前平台
./quiche_engine_all.sh macos x86_64    # macOS Intel
./quiche_engine_all.sh macos arm64     # macOS Apple Silicon
./quiche_engine_all.sh linux x86_64    # Linux

# 或使用 build_client.sh 自动构建（推荐）
# 它会自动检测并构建缺失的 quiche_engine 库
```

### 2. 安装系统依赖

**重要说明**：
- ✅ **服务器**需要系统安装 libev（使用 quiche C API + libev）
- ❌ **客户端**不需要系统 libev（libquiche_engine.a 已静态包含 libev）

#### macOS（仅服务器需要）
```bash
brew install libev
```

#### Ubuntu/Debian（仅服务器需要）
```bash
sudo apt-get install libev-dev
```

#### CentOS/RHEL（仅服务器需要）
```bash
sudo yum install libev-devel
```

**为什么客户端不需要？**
```
libquiche_engine.a = libquiche + libev + BoringSSL（已静态编译）
客户端链接：-L./lib -lquiche -lpthread -ldl -lm（无 -lev）
服务器链接：-L./lib -lquiche -lev -lpthread -ldl -lm（有 -lev）
```

## 🚀 快速开始

### 方法一：使用主 Makefile（推荐）

```bash
# 构建所有
make

# 或分别构建
make server
make client
```

### 方法二：使用独立 Makefile

```bash
# 构建服务器（仅 Linux/macOS）
make -f Makefile.server

# 构建客户端（当前平台）
make -f Makefile.client

# 构建客户端（指定平台）
make -f Makefile.client PLATFORM=macos ARCH=x86_64
make -f Makefile.client PLATFORM=linux ARCH=arm64
```

### 方法三：跨平台批量构建（客户端）⭐

使用 `build_client.sh` 脚本进行跨平台构建：

```bash
# 查看帮助
./build_client.sh --help

# 单平台构建
./build_client.sh macos x86_64
./build_client.sh ios arm64
./build_client.sh android arm64-v8a
./build_client.sh linux x86_64

# 构建所有架构
./build_client.sh ios all          # arm64 + x86_64
./build_client.sh android all      # 所有 Android 架构
./build_client.sh macos all        # arm64 + x86_64

# 构建当前主机平台
./build_client.sh all

# 同时构建多个平台
./build_client.sh ios arm64 android arm64-v8a linux x86_64
```

## 📱 支持的平台和架构

| 平台    | 架构                                            | 说明           |
|---------|------------------------------------------------|----------------|
| macOS   | `x86_64`, `arm64`                             | Intel / Apple Silicon |
| iOS     | `arm64` (设备), `x86_64` (模拟器)              | 需要 Xcode     |
| Android | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`   | 需要 NDK       |
| Linux   | `x86_64`, `arm64`                             | GNU/Linux      |

**注意**：服务器仅支持 Linux/macOS。

## 🎮 运行示例

### 1. 启动服务器

```bash
./quic-server <监听地址> <端口>

# 示例：监听所有接口的 4433 端口
./quic-server 0.0.0.0 4433

# 或使用 Makefile
make run-server HOST=0.0.0.0 PORT=4433
```

### 2. 运行客户端

```bash
./quic-client-<platform>-<arch> <服务器地址> <端口>

# macOS Intel
./quic-client-macos-x86_64 127.0.0.1 4433

# macOS Apple Silicon
./quic-client-macos-arm64 127.0.0.1 4433

# Linux
./quic-client-linux-x86_64 192.168.1.100 4433

# 或使用 Makefile
make run-client HOST=127.0.0.1 PORT=4433
```

### 3. 完整测试流程

**终端 1 - 启动服务器：**
```bash
cd quiche/quic-demo
./quic-server 127.0.0.1 4433
```

**终端 2 - 运行客户端：**
```bash
cd quiche/quic-demo
./quic-client-macos-x86_64 127.0.0.1 4433
```

## 📊 输出示例

### 客户端输出

```
QUIC Client Demo - Bidirectional Data Transfer (Polling Mode)
=============================================================
Upload:   200KB/sec for 5 seconds
Download: Polling for data from server
-------------------------------------------------------------
Connecting to 127.0.0.1:4433...

Starting event loop...

✓ Connection established: hq-interop
✓ Starting data reception polling thread...
✓ Starting data transmission (200KB per second for 5 seconds)...
✓ Received 13500 bytes from server (total received: 13500 bytes)
✓ Sent 204800 bytes in round 1 (total sent: 204800 bytes)
✓ Received 27000 bytes from server (total received: 40500 bytes)
✓ Sent 204800 bytes in round 2 (total sent: 409600 bytes)
...
✓ Data transmission completed. Total sent: 1024000 bytes

⏱ Waiting 8 seconds for server to complete sending remaining data...
  1/8 seconds...
  2/8 seconds...
  ...
  8/8 seconds...

✓ Connection closed

============================================================
Final Statistics
============================================================
Total received from server: 481950 bytes

Connection Statistics:
  Packets sent:     973
  Packets received: 391
  Packets lost:     0
  Bytes sent:       938125
  Bytes received:   500137
  RTT:              0.613578 ms
  CWND:             216000 bytes
============================================================

Cleaning up...
✓ Done
```

### 服务器输出

```
listening on 0.0.0.0:4433
version negotiation
new connection
quiche: connection established: proto=Ok("hq-interop") ...

✓ Received 1311 bytes on stream 4
=== Starting to send 1.5MB/sec to client for 5 seconds ===
✓ Sent 13500 bytes to client in round 0 (total: 13500 bytes)
✓ Received 1310 bytes on stream 4
✓ Sent 27000 bytes to client in round 1 (total: 40500 bytes)
...

connection closed, recv=896470 sent=481950 lost=0 rtt=613578ns cwnd=216000
```

## ⚙️ 高级配置

### 调整 QUIC 参数

客户端参数在 `src/client.cpp` 第 236-249 行：

```cpp
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = static_cast<uint64_t>(30000);          // 30秒
config[ConfigKey::MAX_UDP_PAYLOAD_SIZE] = static_cast<uint64_t>(1350);       // 1350字节
config[ConfigKey::INITIAL_MAX_DATA] = static_cast<uint64_t>(100000000);      // 100MB
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = static_cast<uint64_t>(50000000);   // 50MB
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE] = static_cast<uint64_t>(50000000);  // 50MB
config[ConfigKey::INITIAL_MAX_STREAMS_BIDI] = static_cast<uint64_t>(100);    // 100个流
config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = true;
config[ConfigKey::ENABLE_DEBUG_LOG] = false;  // 改为 true 启用调试
```

服务器参数在 `src/server.c` 第 579-588 行。

### 调整传输速率

**客户端上传速率** (`src/client.cpp:88`)：
```cpp
const size_t CHUNK_SIZE = 200 * 1024;  // 200KB/秒
```

**服务器下载速率** (`src/server.c:139`)：
```c
#define DATA_SIZE (1500 * 1024)  // 1.5MB/秒
```

### 调整等待时间

客户端完成发送后的等待时间 (`src/client.cpp:150`)：
```cpp
for (int i = 0; i < 8 && !should_stop.load(); i++) {  // 8秒
```

## 🧹 清理

```bash
# 清理所有构建产物
make clean

# 仅清理服务器
make -f Makefile.server clean

# 仅清理客户端
make -f Makefile.client clean

# 清理特定平台/架构的客户端
make -f Makefile.client PLATFORM=ios ARCH=arm64 clean-target
```

## 🔍 故障排除

### 编译问题

#### 问题：libev 链接失败
```
Undefined symbols: "_ev_default_loop", "_ev_io_start"...
```
**解决**：安装 libev
```bash
# macOS
brew install libev

# Ubuntu/Debian
sudo apt-get install libev-dev
```

#### 问题：找不到 quiche_engine.h
```
fatal error: 'quiche_engine.h' file not found
```
**解决**：先构建 quiche_engine 库
```bash
cd ../.. && ./quiche_engine_all.sh macos x86_64
# 或直接使用
./build_client.sh macos x86_64  # 会自动构建依赖
```

#### 问题：libquiche_engine.a 不存在
```
✗ Error: quiche_engine library not found
```
**解决**：使用 `build_client.sh` 自动构建
```bash
./build_client.sh macos x86_64  # 自动处理依赖
```

### 运行问题

#### 问题：连接超时
```
⚠ Timeout reached, closing connection...
```
**可能原因**：
1. 服务器未启动或地址错误
2. 防火墙阻止 UDP 端口
3. 网络不可达

**解决**：
```bash
# 检查服务器是否运行
ps aux | grep quic-server

# 检查端口是否监听（macOS）
lsof -nP -iUDP:4433

# 检查端口是否监听（Linux）
sudo netstat -tulpn | grep 4433

# 测试本地回环
./quic-server 127.0.0.1 4433
./quic-client-macos-x86_64 127.0.0.1 4433
```

#### 问题：数据传输不完整
```
Total received from server: 481950 bytes  (expected: 7500000 bytes)
```
**原因**：服务器端流控窗口限制（QUICHE_ERR_DONE）

**改进方向**：
1. 增加服务器的流控窗口配置
2. 减小服务器的发送块大小
3. 实现服务器端的重试逻辑

### 调试模式

启用 QUIC 调试日志：

**客户端** (`src/client.cpp:249`)：
```cpp
config[ConfigKey::ENABLE_DEBUG_LOG] = true;  // 启用调试
```

**服务器** (`src/server.c:594`)：
```c
quiche_enable_debug_logging(debug_log, NULL);  // 取消注释
```

**SSLKEYLOG 环境变量**：
```bash
SSLKEYLOGFILE=/tmp/keys.log ./quic-client 127.0.0.1 4433
SSLKEYLOGFILE=/tmp/keys.log ./quic-server 0.0.0.0 4433
```

## 📦 Makefile 目标

| 目标 | 描述 |
|------|------|
| `make` 或 `make all` | 构建服务器和客户端 |
| `make server` | 仅构建服务器 |
| `make client` | 仅构建客户端（当前平台） |
| `make clean` | 清理所有构建产物 |
| `make run-server HOST=... PORT=...` | 构建并运行服务器 |
| `make run-client HOST=... PORT=...` | 构建并运行客户端 |
| `make help` | 显示帮助信息 |

## 🔄 更新 libquiche.a

如果修改了 quiche 库，需要重新构建：

```bash
# 从 quiche 根目录
cd ../../..

# 使用 FFI 特性构建
cargo build --release --features ffi,boringssl-vendored --no-default-features

# 复制到 quic-demo
cp target/release/libquiche.a quiche/quic-demo/lib/

# 重新构建示例
cd quiche/quic-demo
make clean
make
```

## ⚠️ 已知问题

1. **macOS 版本警告**：链接时可能出现版本警告，可以忽略：
   ```
   ld: warning: object file was built for newer 'macOS' version (15.2) than being linked (14.0)
   ```

2. **数据传输不完整**：由于流控限制，服务器可能无法发送完整的 7.5MB 数据。这是演示性质的问题，生产环境需要实现重传逻辑。

3. **参数警告**：编译时的未使用参数警告是正常的，不影响功能。

## 🎯 性能优化建议

1. **增加拥塞窗口**：
   - 修改 `INITIAL_MAX_DATA` 参数
   - 观察 `CWND` 统计值变化

2. **调整发送速率**：
   - 根据网络条件调整 `CHUNK_SIZE`
   - 监控丢包率（Packets lost）

3. **优化等待时间**：
   - 根据实际传输量调整客户端等待时间
   - 实现基于事件的完成检测而非固定等待

4. **流控优化**：
   - 增加 `INITIAL_MAX_STREAM_DATA_BIDI_REMOTE`
   - 减小服务器的单次发送块大小

## 🔐 证书说明

`certs/` 目录包含测试用自签名证书：
- `cert.crt` - X.509 证书
- `cert.key` - RSA 私钥

**⚠️ 警告**：这些证书仅用于测试，**不可用于生产环境**！

### 生成新证书

```bash
cd certs
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout cert.key -out cert.crt -days 365 \
  -subj "/C=CN/ST=Beijing/L=Beijing/O=Test/CN=localhost"
```

## 📚 参考资源

- **Cloudflare quiche**: https://github.com/cloudflare/quiche
- **quiche 文档**: https://docs.rs/quiche/
- **QUIC 协议 (RFC 9000)**: https://www.rfc-editor.org/rfc/rfc9000.html
- **HTTP/3 (RFC 9114)**: https://www.rfc-editor.org/rfc/rfc9114.html
- **libev 文档**: http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod

## 💡 使用技巧

1. **性能测试**：使用客户端测试连接延迟
2. **开发调试**：修改源码后使用 `make` 快速重新编译
3. **集成开发**：在自己的项目中链接 `lib/libquiche.a`
4. **端口冲突**：如果 4433 被占用，尝试 4434、5433 等

## 📄 许可证

本代码是 quiche 项目的一部分，遵循 BSD 2-Clause 许可证。

详见 `src/server.c` 和 `src/client.cpp` 中的许可证声明。

## 🤝 贡献

如有问题或改进建议，请访问 quiche 主仓库：
https://github.com/cloudflare/quiche

---

**最后更新**：2025-11-07
**quiche 版本**：0.24.6
**编译器**：GCC/Clang 兼容
**支持平台**：macOS, Linux, iOS, Android
