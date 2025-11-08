# QuicheEngine C++ API 文档

## 概述

QuicheEngine 是基于 Cloudflare quiche 库的 C++ QUIC 客户端封装，提供简洁易用的面向对象接口。

**核心特性：**
- 同步连接接口 - `connect()` 阻塞直到连接成功或超时
- 清晰的生命周期管理 - 配置、回调、连接分离
- 支持重连 - `close()` 后保留配置和回调
- 线程安全 - 内部使用事件循环处理网络IO

---

## 快速开始

```cpp
#include <quiche_engine.h>
using namespace quiche;

// 1. 创建引擎对象（无参构造）
QuicheEngine engine;

// 2. 配置 QUIC 参数
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = ConfigValue(30000);  // 30秒
config[ConfigKey::INITIAL_MAX_DATA] = ConfigValue(10000000);
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = ConfigValue(1000000);
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE] = ConfigValue(1000000);

engine.open(config);

// 3. 设置事件回调
engine.setEventCallback([](QuicheEngine* eng, EngineEvent event,
                           const EventData& data, void* user_data) {
    switch (event) {
        case EngineEvent::CONNECTED:
            std::cout << "Connected: " << data.str_val << std::endl;
            break;
        case EngineEvent::STREAM_READABLE:
            // 读取数据
            break;
        case EngineEvent::CONNECTION_CLOSED:
            std::cout << "Connection closed" << std::endl;
            break;
        default:
            break;
    }
}, nullptr);

// 4. 同步连接服务器（阻塞直到成功或超时）
std::string cid = engine.connect("quic.example.com", "4433", 10000);
if (cid.empty()) {
    std::cerr << "Failed: " << engine.getLastError() << std::endl;
    return 1;
}

std::cout << "Connected! CID: " << cid << std::endl;

// 5. 发送数据
const char* request = "GET /\r\n";
engine.write(reinterpret_cast<const uint8_t*>(request), strlen(request), true);

// 6. 读取响应
uint8_t buf[65535];
bool fin = false;
while (!fin) {
    ssize_t len = engine.read(buf, sizeof(buf), fin);
    if (len > 0) {
        std::cout.write(reinterpret_cast<char*>(buf), len);
    } else if (len == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 7. 关闭连接
engine.close(0, "Completed");
```

---

## API 详细说明

### 构造与析构

#### `QuicheEngine()`
无参构造函数，创建未初始化的引擎对象。

**使用场景：**
- 延迟配置的应用
- 需要在对象创建后再设置参数

**示例：**
```cpp
QuicheEngine engine;  // 不分配网络资源
```

#### `~QuicheEngine()`
析构函数，自动关闭连接并释放所有资源。

---

### 配置与初始化

#### `bool open(const ConfigMap& config)`
设置 QUIC 配置参数。

**参数：**
- `config` - QUIC配置映射表

**返回值：**
- `true` - 成功
- `false` - 失败（目前总是返回true）

**前置条件：** 无
**后置状态：** `mIsOpened = true`

**配置选项：**

| ConfigKey | 类型 | 默认值 | 说明 |
|-----------|------|--------|------|
| MAX_IDLE_TIMEOUT | uint64_t | 5000 | 空闲超时（毫秒） |
| MAX_UDP_PAYLOAD_SIZE | uint64_t | 1350 | 最大UDP载荷大小 |
| INITIAL_MAX_DATA | uint64_t | 10000000 | 初始最大数据量 |
| INITIAL_MAX_STREAM_DATA_BIDI_LOCAL | uint64_t | 1000000 | 双向流本地最大数据 |
| INITIAL_MAX_STREAM_DATA_BIDI_REMOTE | uint64_t | 1000000 | 双向流远程最大数据 |
| INITIAL_MAX_STREAM_DATA_UNI | uint64_t | 1000000 | 单向流最大数据 |
| INITIAL_MAX_STREAMS_BIDI | uint64_t | 100 | 最大双向流数 |
| INITIAL_MAX_STREAMS_UNI | uint64_t | 100 | 最大单向流数 |
| DISABLE_ACTIVE_MIGRATION | bool | true | 禁用主动迁移 |
| ENABLE_DEBUG_LOG | bool | false | 启用调试日志 |

**示例：**
```cpp
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = ConfigValue(30000);
config[ConfigKey::INITIAL_MAX_DATA] = ConfigValue(10000000);
config[ConfigKey::ENABLE_DEBUG_LOG] = ConfigValue(true);

engine.open(config);
```

---

#### `bool setEventCallback(EventCallback callback, void* user_data = nullptr)`
设置事件回调函数。

**参数：**
- `callback` - 回调函数
- `user_data` - 用户自定义数据指针（可选）

**返回值：**
- `true` - 成功
- `false` - 失败（目前总是返回true）

**前置条件：** 无
**后置状态：** `mHasCallback = true`

**回调签名：**
```cpp
using EventCallback = std::function<void(
    QuicheEngine* engine,
    EngineEvent event,
    const EventData& event_data,
    void* user_data
)>;
```

**事件类型：**

| EngineEvent | EventData 类型 | 说明 |
|-------------|---------------|------|
| CONNECTED | STRING | 协商的应用协议（如 "hq-interop"） |
| CONNECTION_CLOSED | NONE | 连接已关闭 |
| STREAM_READABLE | UINT64 | 流ID可读 |
| STREAM_WRITABLE | UINT64 | 流ID可写 |
| DATAGRAM_RECEIVED | NONE | 收到数据报 |
| ERROR | STRING | 错误消息 |

**示例：**
```cpp
void onEvent(QuicheEngine* eng, EngineEvent event,
             const EventData& data, void* user_data) {
    switch (event) {
        case EngineEvent::CONNECTED:
            std::cout << "Protocol: " << data.str_val << std::endl;
            break;
        case EngineEvent::STREAM_READABLE:
            std::cout << "Stream " << data.uint_val << " readable" << std::endl;
            break;
    }
}

engine.setEventCallback(onEvent, nullptr);
```

---

### 连接管理

#### `std::string connect(const std::string& host, const std::string& port, uint64_t timeout_ms = 5000)`
**同步连接**服务器，阻塞直到连接建立或超时。

**参数：**
- `host` - 服务器主机名或IP地址
- `port` - 服务器端口号
- `timeout_ms` - 超时时间（毫秒），0表示使用默认5000ms

**返回值：**
- 成功：8字符十六进制连接ID（如 "a1b2c3d4"）
- 失败：空字符串，可通过 `getLastError()` 获取错误信息

**前置条件：**
- 必须先调用 `open()` 设置配置
- 必须先调用 `setEventCallback()` 设置回调

**后置状态：** `mIsConnected = true` (成功时)

**错误处理：**
```cpp
std::string cid = engine.connect("server.com", "4433", 10000);
if (cid.empty()) {
    std::cerr << "Connection failed: " << engine.getLastError() << std::endl;
    // 可能的错误：
    // - "Must call open() before connect()"
    // - "Must call setEventCallback() before connect()"
    // - "Connection timeout after 10000ms"
    // - "Failed to resolve hostname"
    // - DNS解析、socket创建等错误
}
```

**重连示例：**
```cpp
// 首次连接
std::string cid = engine.connect("server.com", "4433");
// ... 使用连接 ...
engine.close();

// 重连（配置和回调已保留）
std::string cid2 = engine.connect("server.com", "4433");
```

---

#### `void close(uint64_t app_error = 0, const std::string& reason = "")`
优雅关闭连接。

**参数：**
- `app_error` - 应用层错误码（0表示正常关闭）
- `reason` - 关闭原因字符串

**行为：**
1. 发送 CONNECTION_CLOSE 帧到对端
2. 停止事件循环
3. 释放网络资源（socket、quiche连接对象）
4. **保留** 配置和回调，允许重连

**示例：**
```cpp
// 正常关闭
engine.close(0, "User logout");

// 异常关闭
engine.close(1, "Internal error");

// 简单关闭
engine.close();
```

---

### 数据传输

#### `ssize_t write(const uint8_t* data, size_t len, bool fin)`
向默认流（Stream ID 4）写入数据。

**参数：**
- `data` - 数据缓冲区指针
- `len` - 数据长度（最大 65536 字节）
- `fin` - 是否为流的最后数据

**返回值：**
- 成功：写入字节数（等于len）
- 失败：-1

**特性：**
- 线程安全（通过命令队列）
- 非阻塞（立即返回，实际发送在事件循环中）

**示例：**
```cpp
const char* request = "GET / HTTP/0.9\r\n";
ssize_t written = engine.write(
    reinterpret_cast<const uint8_t*>(request),
    strlen(request),
    true  // FIN标志
);

if (written < 0) {
    std::cerr << "Write failed: " << engine.getLastError() << std::endl;
}
```

---

#### `ssize_t read(uint8_t* buf, size_t buf_len, bool& fin)`
从默认流（Stream ID 4）读取数据。

**参数：**
- `buf` - 接收缓冲区
- `buf_len` - 缓冲区大小
- `fin` - 输出参数，是否收到FIN标志

**返回值：**
- `> 0` - 读取到的字节数
- `0` - 当前无数据可读（需要等待）
- `-1` - 致命错误

**特性：**
- 线程安全
- 非阻塞（无数据时返回0）
- 流式读取（按接收顺序）

**示例：**
```cpp
uint8_t buffer[4096];
bool fin = false;

while (!fin) {
    ssize_t len = engine.read(buffer, sizeof(buffer), fin);

    if (len > 0) {
        // 处理数据
        std::cout.write(reinterpret_cast<char*>(buffer), len);
    } else if (len == 0) {
        // 无数据，短暂等待
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } else {
        // 错误
        std::cerr << "Read error" << std::endl;
        break;
    }
}
```

---

### 状态查询

#### `bool isConnected() const`
检查连接是否已建立。

**返回值：**
- `true` - 连接已建立
- `false` - 未连接或连接已关闭

#### `bool isRunning() const`
检查事件循环是否运行中。

**返回值：**
- `true` - 事件循环运行中
- `false` - 事件循环未启动或已停止

#### `std::string getLastError() const`
获取最后一次错误消息。

**返回值：** 错误描述字符串（空表示无错误）

#### `std::string getScid() const`
获取源连接ID（Source Connection ID）。

**返回值：** 8字符十六进制字符串

#### `EngineStats getStats() const`
获取连接统计信息。

**返回值：** `EngineStats` 结构体

**字段：**
```cpp
struct EngineStats {
    size_t packets_sent;        // 发送数据包数
    size_t packets_received;    // 接收数据包数
    size_t bytes_sent;          // 发送字节数
    size_t bytes_received;      // 接收字节数
    size_t packets_lost;        // 丢包数
    uint64_t rtt_ns;            // RTT（纳秒）
    uint64_t cwnd;              // 拥塞窗口
};
```

**示例：**
```cpp
EngineStats stats = engine.getStats();
std::cout << "RTT: " << stats.rtt_ns / 1000000.0 << " ms" << std::endl;
std::cout << "Sent: " << stats.bytes_sent << " bytes" << std::endl;
std::cout << "Loss rate: " << (stats.packets_lost * 100.0 / stats.packets_sent)
          << "%" << std::endl;
```

---

## 完整使用示例

### 示例1：简单HTTP/0.9请求

```cpp
#include <quiche_engine.h>
#include <iostream>
#include <thread>

using namespace quiche;

int main() {
    // 1. 创建和配置
    QuicheEngine engine;

    ConfigMap config;
    config[ConfigKey::MAX_IDLE_TIMEOUT] = ConfigValue(30000);
    config[ConfigKey::INITIAL_MAX_DATA] = ConfigValue(10000000);
    engine.open(config);

    // 2. 设置回调
    bool connected = false;
    engine.setEventCallback([&](QuicheEngine* eng, EngineEvent event,
                               const EventData& data, void* ud) {
        if (event == EngineEvent::CONNECTED) {
            connected = true;
            std::cout << "Connected with " << data.str_val << std::endl;
        } else if (event == EngineEvent::CONNECTION_CLOSED) {
            std::cout << "Connection closed" << std::endl;
        }
    }, nullptr);

    // 3. 连接
    std::string cid = engine.connect("quic.aiortc.org", "4433", 10000);
    if (cid.empty()) {
        std::cerr << "Failed: " << engine.getLastError() << std::endl;
        return 1;
    }

    std::cout << "CID: " << cid << std::endl;

    // 4. 发送请求
    const char* request = "GET /\r\n";
    engine.write(reinterpret_cast<const uint8_t*>(request),
                 strlen(request), true);

    // 5. 读取响应
    uint8_t buf[65535];
    bool fin = false;
    size_t total = 0;

    while (!fin) {
        ssize_t len = engine.read(buf, sizeof(buf), fin);
        if (len > 0) {
            total += len;
            std::cout.write(reinterpret_cast<char*>(buf), len);
        } else if (len == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            break;
        }
    }

    // 6. 打印统计
    EngineStats stats = engine.getStats();
    std::cout << "\n--- Statistics ---" << std::endl;
    std::cout << "Received: " << total << " bytes" << std::endl;
    std::cout << "RTT: " << stats.rtt_ns / 1000000.0 << " ms" << std::endl;
    std::cout << "Packets sent: " << stats.packets_sent << std::endl;

    // 7. 关闭
    engine.close(0, "Completed");

    return 0;
}
```

### 示例2：可重连的客户端

```cpp
class QuicClient {
public:
    QuicClient() {
        // 配置
        config_[ConfigKey::MAX_IDLE_TIMEOUT] = ConfigValue(30000);
        config_[ConfigKey::INITIAL_MAX_DATA] = ConfigValue(10000000);
        engine_.open(config_);

        // 设置回调
        engine_.setEventCallback(
            [this](QuicheEngine* eng, EngineEvent event,
                   const EventData& data, void* ud) {
                this->onEvent(event, data);
            }, nullptr);
    }

    bool connect(const std::string& host, const std::string& port) {
        std::string cid = engine_.connect(host, port, 10000);
        if (cid.empty()) {
            last_error_ = engine_.getLastError();
            return false;
        }
        host_ = host;
        port_ = port;
        return true;
    }

    bool reconnect() {
        if (host_.empty()) return false;
        return connect(host_, port_);
    }

    void disconnect() {
        engine_.close(0, "User disconnect");
    }

    ssize_t send(const std::string& data, bool fin = false) {
        return engine_.write(
            reinterpret_cast<const uint8_t*>(data.c_str()),
            data.length(), fin);
    }

    std::string receive() {
        uint8_t buf[4096];
        bool fin = false;
        std::string result;

        while (true) {
            ssize_t len = engine_.read(buf, sizeof(buf), fin);
            if (len > 0) {
                result.append(reinterpret_cast<char*>(buf), len);
            }
            if (len <= 0 || fin) break;
        }
        return result;
    }

private:
    void onEvent(EngineEvent event, const EventData& data) {
        // 处理事件
    }

    QuicheEngine engine_;
    ConfigMap config_;
    std::string host_;
    std::string port_;
    std::string last_error_;
};

// 使用
QuicClient client;
if (client.connect("server.com", "4433")) {
    client.send("Hello", true);
    std::string response = client.receive();
    client.disconnect();

    // 可以重连
    if (client.reconnect()) {
        // ...
    }
}
```

---

## API 调用顺序

### 正确的调用顺序

```
QuicheEngine()        // 1. 构造
    ↓
open(config)          // 2. 设置配置
    ↓
setEventCallback()    // 3. 设置回调
    ↓
connect()             // 4. 连接（阻塞）
    ↓
write() / read()      // 5. 数据传输
    ↓
close()               // 6. 关闭
    ↓
[可选] connect()      // 7. 重连
```

### 错误的调用顺序

```cpp
// ❌ 错误：未调用 open()
QuicheEngine engine;
engine.setEventCallback(callback, nullptr);
engine.connect("host", "port");  // 返回空字符串，错误："Must call open() before connect()"

// ❌ 错误：未调用 setEventCallback()
QuicheEngine engine;
engine.open(config);
engine.connect("host", "port");  // 返回空字符串，错误："Must call setEventCallback() before connect()"

// ✅ 正确
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(callback, nullptr);
engine.connect("host", "port");  // 成功
```

---

## 线程安全性

- **线程安全的方法：** `write()`, `read()`, 所有const查询方法
- **非线程安全的方法：** `open()`, `setEventCallback()`, `connect()`, `close()`

**推荐用法：**
- 在主线程中调用 `open()`, `setEventCallback()`, `connect()`, `close()`
- 在任意线程中调用 `write()`, `read()`
- 回调函数在内部事件循环线程中执行，避免长时间阻塞

---

## 性能优化建议

1. **批量读取：** 使用足够大的缓冲区（建议 ≥ 4KB）
2. **避免频繁小包：** 合并多个小数据后再write()
3. **配置优化：**
   - 大文件传输：增大 `INITIAL_MAX_DATA`
   - 低延迟：减小 `MAX_IDLE_TIMEOUT`
4. **连接复用：** 使用 `close()` + `connect()` 重连，避免重新创建对象

---

## 故障排查

### 连接超时
```cpp
std::string cid = engine.connect("host", "port", 10000);
if (cid.empty()) {
    std::string error = engine.getLastError();
    if (error.find("timeout") != std::string::npos) {
        // 网络延迟高或服务器无响应
        // 解决方案：增加 timeout_ms
    }
}
```

### DNS解析失败
```cpp
if (error.find("Failed to resolve") != std::string::npos) {
    // 主机名无效或DNS服务器问题
    // 解决方案：检查主机名拼写，或使用IP地址
}
```

### 启用调试日志
```cpp
config[ConfigKey::ENABLE_DEBUG_LOG] = ConfigValue(true);
engine.open(config);
// 日志输出到 stderr
```

---

## 编译和链接

### 头文件
```cpp
#include <quiche_engine.h>
```

### 链接库
```bash
# macOS
c++ -std=c++17 client.cpp -I<path>/quiche/engine/include \
    -L<path>/lib -lquiche_engine -lquiche -lev \
    -framework CoreFoundation -framework Security

# Linux
g++ -std=c++17 client.cpp -I<path>/quiche/engine/include \
    -L<path>/lib -lquiche_engine -lquiche -lev -ldl -lpthread
```

### CMake示例
```cmake
find_library(QUICHE_ENGINE_LIB quiche_engine)
find_library(QUICHE_LIB quiche)
find_library(EV_LIB ev)

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${QUICHE_INCLUDE_DIR})
target_link_libraries(client ${QUICHE_ENGINE_LIB} ${QUICHE_LIB} ${EV_LIB})
```

---

## 更新日志

### v3.0 (当前版本)
- **破坏性变更：** 移除 `start()` 和 `shutdown()` 方法
- **新增：** `open()` 方法，独立设置配置
- **新增：** `connect()` 同步连接接口
- **新增：** `close()` 方法，替代 `shutdown()`
- **修改：** 构造函数改为无参数
- **优化：** 支持重连（close后保留配置和回调）

### v2.0 (历史版本)
- 初始版本，使用 `QuicheEngine(host, port, config)` 构造
- 使用 `start()` 异步连接
- 使用 `shutdown()` 关闭

---

## 常见问题 (FAQ)

**Q: connect() 会阻塞多久？**
A: 最长阻塞 timeout_ms 毫秒，成功建立连接后立即返回。

**Q: close() 后可以重新 connect() 吗？**
A: 可以。配置和回调会被保留，直接调用 `connect()` 即可重连。

**Q: 如何处理连接断开？**
A: 在事件回调中监听 `EngineEvent::CONNECTION_CLOSED`，然后调用 `connect()` 重连。

**Q: write() 是同步还是异步？**
A: 异步。数据被加入发送队列后立即返回，实际发送在事件循环中完成。

**Q: 为什么read()返回0？**
A: 当前无数据可读。应该等待一段时间后重试，或在 `STREAM_READABLE` 回调中读取。

**Q: 支持多个并发流吗？**
A: 当前版本仅支持默认流（ID 4）。多流支持计划在未来版本中添加。

---

## 许可证

Copyright (C) 2025, Cloudflare, Inc.
All rights reserved.
