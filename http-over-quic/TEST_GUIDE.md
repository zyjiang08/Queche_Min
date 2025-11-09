# HTTP-over-QUIC 测试指南

## 项目概述

本项目实现了基于 QUIC 协议的 HTTP/1.1 文件传输服务，包含服务器和客户端两部分。

## 目录结构

```
http-over-quic/
├── Makefile              # 构建配置
├── README.md             # 项目说明
├── TEST_GUIDE.md         # 本测试指南
├── cert.crt              # TLS证书（自签名）
├── cert.key              # TLS私钥
├── http-client           # HTTP客户端可执行文件
├── http-server           # HTTP服务器可执行文件
├── src/
│   ├── client.cpp        # 客户端源码（使用QuicheEngine C++封装）
│   └── server.c          # 服务器源码（使用quiche C API）
├── include/
│   ├── http_protocol.h   # HTTP协议定义
│   ├── quiche.h          # Quiche库头文件
│   └── uthash.h          # 哈希表工具
├── lib/
│   └── libquiche.a       # Quiche静态库
├── data/                 # 测试文件目录
│   ├── test.bin          # 1MB测试文件
│   ├── small.bin         # 4KB测试文件
│   └── tiny.bin          # 1KB测试文件
└── build/                # 编译中间文件
```

## 构建依赖

### 系统要求
- macOS 或 Linux
- GCC/Clang 编译器
- libev (事件循环库)
- OpenSSL (SHA256计算)

### 依赖库
项目依赖于上级目录的 quiche 库：
```bash
# 确保已编译 quiche_engine 库
cd ..
./quiche_engine_all.sh macos x86_64  # 或 linux x86_64
```

## 编译

### 编译所有组件
```bash
make all
```

### 单独编译
```bash
make client  # 只编译客户端
make server  # 只编译服务器
make clean   # 清理编译文件
```

### 编译输出
- `http-client` (约2.1MB) - HTTP客户端
- `http-server` (约2.1MB) - HTTP服务器

## 运行测试

### 1. 准备测试文件

项目自带测试文件在 `data/` 目录：
```bash
ls -lh data/
# test.bin  (1MB)
# small.bin (4KB)
# tiny.bin  (1KB)
```

也可以创建自定义测试文件：
```bash
# 创建10KB测试文件
dd if=/dev/zero of=data/custom.bin bs=1024 count=10
```

### 2. 启动服务器

```bash
./http-server <host> <port>
```

示例：
```bash
# 监听本地8443端口
./http-server 127.0.0.1 8443

# 输出示例：
# HTTP over QUIC Server (Direct quiche API)
# ==========================================
# Listen address: 127.0.0.1:8443
# Root directory: data
# ==========================================
#
# Waiting for client connection...
```

**注意**: 服务器默认从 `data/` 目录提供文件。

### 3. 运行客户端

```bash
./http-client <host> <port> <uri> [output_file]
```

参数说明：
- `host`: 服务器地址
- `port`: 服务器端口
- `uri`: 请求的文件路径（相对于data/）
- `output_file`: 保存位置（可选，默认为downloaded.bin）

示例：
```bash
# 下载 data/test.bin 到 output.bin
./http-client 127.0.0.1 8443 /test.bin output.bin

# 下载小文件
./http-client 127.0.0.1 8443 /small.bin small_downloaded.bin
```

### 4. 验证下载结果

客户端会自动验证下载文件的 SHA256 哈希：

```bash
# 客户端输出示例：
HTTP over QUIC Client
=====================
Server:      127.0.0.1:8443
Request URI: /small.bin
Output file: small_downloaded.bin
=====================

Connecting to 127.0.0.1:8443...
✓ Connected: hq-interop
✓ Connection established, sending HTTP GET request...
✓ Request sent (115 bytes)

✓ HTTP Response received:
  Status: 200 OK
  Content-Type: application/octet-stream
  Content-Length: 4096
  X-Content-SHA256: ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7

✓ Saving to: small_downloaded.bin
✓ SHA256 verification enabled

=== Integrity Verification ===
  Expected SHA256:   ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7
  Calculated SHA256: ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7
  ✓ Integrity verification PASSED

✓ Done!
```

手动验证：
```bash
# 比较文件大小
ls -l data/small.bin small_downloaded.bin

# 比较SHA256哈希
shasum -a 256 data/small.bin small_downloaded.bin
```

## 完整测试流程示例

### 终端1: 启动服务器
```bash
cd /path/to/http-over-quic
./http-server 127.0.0.1 8443
```

### 终端2: 测试文件下载
```bash
cd /path/to/http-over-quic

# 测试1: 下载小文件 (4KB)
./http-client 127.0.0.1 8443 /small.bin test1.bin
ls -lh test1.bin
shasum -a 256 data/small.bin test1.bin

# 测试2: 下载1KB文件
./http-client 127.0.0.1 8443 /tiny.bin test2.bin
shasum -a 256 data/tiny.bin test2.bin

# 测试3: 请求不存在的文件 (应返回404)
./http-client 127.0.0.1 8443 /nonexistent.bin test3.bin
```

## 已知限制

### 大文件传输限制

⚠️ **重要**: 当前实现存在大文件传输限制

**问题描述**:
- 服务器只能传输约 2KB 数据后停止
- 原因：缺少可写流事件处理机制

**影响范围**:
- 1KB 文件: ✅ 完全传输
- 4KB 文件: ⚠️ 部分传输 (~50%)
- 1MB 文件: ❌ 传输失败 (~0.2%)

**技术原因**:
QUIC 拥塞窗口初始约 13.5KB，当窗口填满后，服务器需要：
1. 等待客户端 ACK 以打开拥塞窗口
2. 监听流可写事件
3. 继续发送剩余数据

当前 `server.c` 的 `send_http_response()` 函数只在接收到 HTTP 请求时调用一次，不支持事件驱动的增量发送。

**解决方案**:
需要实现完整的可写流处理：
```c
// 需要添加的代码结构
struct pending_transfer {
    uint64_t stream_id;
    uint8_t *data;
    size_t offset;
    size_t total_size;
};

// 在 recv_cb 中处理可写流
quiche_stream_iter *writable = quiche_conn_writable(conn);
while (quiche_stream_iter_next(writable, &stream_id)) {
    // 继续发送 pending_transfer 数据
}
```

### 推荐使用场景

✅ **适用**:
- 小文件传输 (< 2KB)
- HTTP 协议验证
- QUIC 连接测试
- 学习和演示

❌ **不适用**:
- 生产环境大文件传输
- 需要完整文件下载的场景

## 协议特性

### 已实现功能

✅ **QUIC 连接**:
- TLS 1.3 握手
- 连接建立和管理
- 自签名证书支持 (VERIFY_PEER=false)

✅ **HTTP/1.1 协议**:
- GET 请求解析
- HTTP 响应生成
- 标准 HTTP 头部
- 自定义头部 `X-Content-SHA256`

✅ **文件服务**:
- 从 data/ 目录提供文件
- 自动 SHA256 计算
- 404 错误处理
- 400 错误处理

✅ **完整性验证**:
- 服务器端 SHA256 计算
- 客户端端 SHA256 验证
- 自动完整性检查

### HTTP 头部示例

**请求**:
```http
GET /test.bin HTTP/1.1
Accept: */*
Connection: close
Host: localhost
User-Agent: HTTP-over-QUIC-Client/1.0
```

**响应**:
```http
HTTP/1.1 200 OK
Server: HTTP-over-QUIC/1.0
Content-Type: application/octet-stream
Content-Length: 4096
X-Content-SHA256: ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7

[文件数据]
```

## 调试

### 启用 QUIC 调试日志

服务器已内置 QUIC 调试日志，会输出详细的连接信息：
```
quiche: rx pkt Initial dcid=... len=1200 pn=0
quiche: connection established: proto=Ok("hq-interop")
quiche: rx frm STREAM id=4 off=0 len=114
```

### 常见问题排查

**问题1: 连接失败**
```bash
# 检查服务器是否运行
ps aux | grep http-server

# 检查端口是否监听
lsof -i :8443

# 重启服务器
killall http-server
./http-server 127.0.0.1 8443
```

**问题2: 文件找不到 (404)**
```bash
# 确认文件存在
ls -l data/test.bin

# 检查 URI 格式（必须以 / 开头）
./http-client 127.0.0.1 8443 /test.bin output.bin  # ✓ 正确
./http-client 127.0.0.1 8443 test.bin output.bin   # ✗ 错误
```

**问题3: SHA256 不匹配**
```bash
# 手动验证服务器文件哈希
shasum -a 256 data/test.bin

# 检查文件是否完整下载
ls -l data/test.bin downloaded.bin
```

## 性能统计

服务器会在连接关闭时输出统计信息：
```
connection closed, recv=5 sent=5 lost=0 rtt=582310ns cwnd=13500
```

说明：
- `recv`: 接收包数量
- `sent`: 发送包数量
- `lost`: 丢包数量
- `rtt`: 往返时间 (纳秒)
- `cwnd`: 拥塞窗口大小 (字节)

## 开发参考

### 关键代码位置

**服务器** (`src/server.c`):
- `parse_http_request()` (117-141行) - HTTP 请求解析
- `send_http_response()` (145-254行) - HTTP 响应生成
- `calculate_sha256()` (104-113行) - SHA256 计算
- `recv_cb()` (414-647行) - 主事件处理

**客户端** (`src/client.cpp`):
- `onConnected()` (61-89行) - 连接建立回调
- `onData()` (92-250行) - 数据接收处理
- `main()` (322-406行) - 程序入口

### 修改建议

如需支持大文件传输，参考以下改进方向：
1. 在 `conn_io` 结构体中添加 `pending_transfers` 队列
2. 修改 `send_http_response()` 为增量发送
3. 在 `recv_cb()` 中添加可写流处理
4. 实现发送状态跟踪和重试机制

## 许可证

本项目基于 Cloudflare quiche 项目，遵循 BSD-2-Clause 许可证。

## 参考资源

- [Cloudflare quiche](https://github.com/cloudflare/quiche)
- [QUIC 协议规范 (RFC 9000)](https://www.rfc-editor.org/rfc/rfc9000.html)
- [HTTP/3 规范 (RFC 9114)](https://www.rfc-editor.org/rfc/rfc9114.html)
