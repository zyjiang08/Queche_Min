# Quiche Engine API 文档

## 目录
- [1. 架构概览](#1-架构概览)
- [2. 组件关系](#2-组件关系)
- [3. API 接口详解](#3-api-接口详解)
- [4. 事件系统](#4-事件系统)
- [5. 使用示例](#5-使用示例)
- [6. 最佳实践](#6-最佳实践)

---

## 1. 架构概览

### 1.1 系统架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                          │
│                     quic-demo/src/client.cpp                        │
│                     quic-demo/src/server.c                          │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 使用
                             ↓
┌─────────────────────────────────────────────────────────────────────┐
│                    Quiche Engine 公共API层                           │
│              quiche/engine/include/quiche_engine.h                  │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                   QuicheEngine 类                             │  │
│  │  ┌────────────────────────────────────────────────────────┐  │   │
│  │  │  构造/析构   配置   事件回调   数据读写   状态查询           │  │  │
│  │  │  start()  shutdown()  write()  read()  getStats()      │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────────────┘
                             │ PIMPL模式
                             ↓
┌─────────────────────────────────────────────────────────────────────┐
│                  Quiche Engine 实现层                                │
│          quiche/engine/src/quiche_engine_impl.{h,cpp}               │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              QuicheEngineImpl 类                             │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  • 事件循环线程 (libev)                                 │  │  │
│  │  │  • 命令队列 (线程安全)                                  │  │  │
│  │  │  • 流缓冲区管理                                         │  │  │
│  │  │  • Socket I/O (批量/单包)                               │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 调用
                             ↓
┌─────────────────────────────────────────────────────────────────────┐
│                      Quiche 核心库                                   │
│                   quiche/include/quiche.h                           │
│                   quiche/src/lib.rs                                 │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │  • QUIC协议实现 (RFC 9000)                                   │  │
│  │  • TLS 1.3集成 (BoringSSL)                                   │  │
│  │  • 流多路复用                                                │  │
│  │  • 拥塞控制 (CUBIC/BBR/Reno)                                 │  │
│  │  • 丢包恢复                                                  │  │
│  │  • 连接迁移                                                  │  │
│  └──────────────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 依赖
                             ↓
┌─────────────────────────────────────────────────────────────────────┐
│                         底层依赖                                     │
│                                                                      │
│  ┌─────────────┐    ┌──────────────┐    ┌──────────────┐           │
│  │   libev     │    │  BoringSSL   │    │  std::thread │           │
│  │  (事件循环)  │    │  (TLS/加密)   │    │  (C++11线程) │           │
│  └─────────────┘    └──────────────┘    └──────────────┘           │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 线程模型

```
应用线程                        事件循环线程
(Main Thread)                  (Event Loop Thread)
     │                                │
     │  1. 创建QuicheEngine            │
     ├──────────────────────────────→ │
     │                                │
     │  2. setEventCallback()         │
     ├──────────────────────────────→ │
     │                                │
     │  3. start()                    │
     ├──────────────────────────────→ │ 启动事件循环
     │                                │ ev_run()
     │                                ├──────────┐
     │  4. write(data)                │          │
     ├───→ [命令队列] ─────────────────→          │
     │                                │  • Socket I/O
     │                                │  • quiche处理
     │  5. read(buf) ←─ [流缓冲区] ←───          │
     │                                │  • 定时器
     ↓                                ↓          ↓
数据发送线程                      事件回调
(Send Thread)                   (Callbacks)
     │                                │
     │  write()                       │  CONNECTED
     ├──────────────────────────────→ │  STREAM_READABLE
     │                                │  CONNECTION_CLOSED
                                      │  ERROR
数据接收线程                          │
(Recv Thread)                         │
     │                                │
     │  read()                        │
     │←──────────────────────────────┘
```

---

## 2. 组件关系

### 2.1 核心组件

| 组件                    | 位置                              | 职责                                    |
|------------------------|-----------------------------------|----------------------------------------|
| **QuicheEngine**       | `include/quiche_engine.h`         | 公共API，PIMPL包装器                     |
| **QuicheEngineImpl**   | `src/quiche_engine_impl.{h,cpp}`  | 核心实现，事件循环，线程管理              |
| **CommandQueue**       | `src/quiche_engine_impl.h`        | 线程安全的命令队列                       |
| **StreamReadBuffer**   | `src/quiche_engine_impl.h`        | 每个流的读缓冲区                         |
| **libev**              | `deps/libev/`                     | 事件循环库                              |
| **quiche**             | `include/quiche.h`                | QUIC协议核心实现                         |

### 2.2 数据流向

#### 发送路径 (Write Path)
```
应用 write()
    ↓
创建 Command::WRITE
    ↓
压入 CommandQueue (线程安全)
    ↓
触发 ev_async_send()
    ↓
事件循环线程被唤醒
    ↓
processCommands() 处理队列
    ↓
quiche_conn_stream_send()
    ↓
flushEgress()
    ↓
sendmsg/sendmmsg (Socket发送)
```

#### 接收路径 (Read Path)
```
Socket接收 (recvmsg/recvmmsg)
    ↓
quiche_conn_recv() 处理数据包
    ↓
检测可读流 (quiche_conn_readable())
    ↓
quiche_conn_stream_recv() 读取数据
    ↓
写入 StreamReadBuffer (加锁)
    ↓
触发 STREAM_READABLE 事件
    ↓
应用调用 read() 从缓冲区读取
```

---

## 3. API 接口详解

### 3.1 类定义

```cpp
namespace quiche {
    class QuicheEngine {
    public:
        // 构造与析构
        QuicheEngine(const std::string& host, const std::string& port,
                     const ConfigMap& config = ConfigMap());
        ~QuicheEngine();

        // 移动语义（支持）
        QuicheEngine(QuicheEngine&& other) noexcept;
        QuicheEngine& operator=(QuicheEngine&& other) noexcept;

        // 拷贝语义（禁用）
        QuicheEngine(const QuicheEngine&) = delete;
        QuicheEngine& operator=(const QuicheEngine&) = delete;

        // 核心API
        bool setEventCallback(EventCallback callback, void* user_data = nullptr);
        bool start();
        void shutdown(uint64_t app_error = 0, const std::string& reason = "");
        ssize_t write(const uint8_t* data, size_t len, bool fin);
        ssize_t read(uint8_t* buf, size_t buf_len, bool& fin);

        // 状态查询
        bool isConnected() const;
        bool isRunning() const;
        EngineStats getStats() const;
        std::string getLastError() const;
        std::string getScid() const;
    };
}
```

### 3.2 接口详细说明

#### 3.2.1 构造函数

```cpp
QuicheEngine(const std::string& host,
             const std::string& port,
             const ConfigMap& config = ConfigMap())
```

**功能**: 创建QUIC引擎实例

**参数**:
- `host`: 远程主机名或IP地址
- `port`: 远程端口号
- `config`: 配置参数映射（可选）

**配置项**:
| ConfigKey | 类型 | 默认值 | 说明 |
|-----------|------|--------|------|
| `MAX_IDLE_TIMEOUT` | uint64_t | 5000 | 空闲超时（毫秒） |
| `MAX_UDP_PAYLOAD_SIZE` | uint64_t | 1350 | UDP载荷最大尺寸（字节） |
| `INITIAL_MAX_DATA` | uint64_t | 10000000 | 初始最大数据量（10MB） |
| `INITIAL_MAX_STREAM_DATA_BIDI_LOCAL` | uint64_t | 1000000 | 双向流本地数据窗口（1MB） |
| `INITIAL_MAX_STREAM_DATA_BIDI_REMOTE` | uint64_t | 1000000 | 双向流远程数据窗口（1MB） |
| `INITIAL_MAX_STREAM_DATA_UNI` | uint64_t | 1000000 | 单向流数据窗口（1MB） |
| `INITIAL_MAX_STREAMS_BIDI` | uint64_t | 100 | 最大双向流数量 |
| `INITIAL_MAX_STREAMS_UNI` | uint64_t | 100 | 最大单向流数量 |
| `DISABLE_ACTIVE_MIGRATION` | bool | true | 禁用连接迁移 |
| `ENABLE_DEBUG_LOG` | bool | false | 启用调试日志 |

**示例**:
```cpp
// 默认配置
QuicheEngine engine("127.0.0.1", "4433");

// 自定义配置
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;  // 30秒
config[ConfigKey::INITIAL_MAX_DATA] = 100000000;  // 100MB
config[ConfigKey::ENABLE_DEBUG_LOG] = true;
QuicheEngine engine("example.com", "443", config);
```

**内部行为**:
1. 生成8字符随机十六进制SCID（连接ID）
2. 初始化默认流ID为4（客户端发起的双向流）
3. 分配I/O缓冲区（堆内存）
4. 初始化互斥锁和线程相关资源

---

#### 3.2.2 setEventCallback

```cpp
bool setEventCallback(EventCallback callback, void* user_data = nullptr)
```

**功能**: 设置事件回调处理函数

**参数**:
- `callback`: 事件回调函数
- `user_data`: 用户自定义数据指针（可选）

**返回值**:
- `true`: 设置成功
- `false`: 设置失败

**回调函数签名**:
```cpp
using EventCallback = std::function<void(
    QuicheEngine* engine,      // 引擎实例指针
    EngineEvent event,          // 事件类型
    const EventData& event_data, // 事件数据
    void* user_data             // 用户数据
)>;
```

**示例**:
```cpp
void onEvent(QuicheEngine* engine, EngineEvent event,
             const EventData& data, void* user_data) {
    switch (event) {
        case EngineEvent::CONNECTED:
            std::cout << "Connected: " << data.str_val << std::endl;
            break;
        case EngineEvent::CONNECTION_CLOSED:
            std::cout << "Connection closed" << std::endl;
            break;
        case EngineEvent::ERROR:
            std::cerr << "Error: " << engine->getLastError() << std::endl;
            break;
    }
}

engine.setEventCallback(onEvent, nullptr);
```

---

#### 3.2.3 start

```cpp
bool start()
```

**功能**: 启动引擎和事件循环（非阻塞）

**返回值**:
- `true`: 启动成功
- `false`: 启动失败（检查 `getLastError()` 获取详情）

**内部行为**:
1. 创建UDP socket
2. 建立QUIC连接（握手）
3. 启动后台事件循环线程
4. 立即返回（非阻塞）

**示例**:
```cpp
if (!engine.start()) {
    std::cerr << "Failed to start: " << engine.getLastError() << std::endl;
    return 1;
}
std::cout << "Engine started, SCID: " << engine.getScid() << std::endl;
```

**注意事项**:
- `start()` 必须在 `setEventCallback()` 之后调用
- 连接成功后会触发 `CONNECTED` 事件
- 启动后应用可以立即调用 `write()` 和 `read()`

---

#### 3.2.4 shutdown

```cpp
void shutdown(uint64_t app_error = 0, const std::string& reason = "")
```

**功能**: 关闭连接和事件循环（阻塞，等待优雅关闭）

**参数**:
- `app_error`: 应用层错误码（默认0表示正常关闭）
- `reason`: 关闭原因字符串（可选）

**内部行为**:
1. 向命令队列发送 `STOP` 命令
2. 发送QUIC连接关闭帧
3. 等待事件循环线程退出
4. 清理资源

**示例**:
```cpp
// 正常关闭
engine.shutdown();

// 带错误码和原因
engine.shutdown(1001, "Application timeout");
```

**注意事项**:
- `shutdown()` 会阻塞，直到事件循环完全停止
- 析构函数会自动调用 `shutdown()`
- 关闭后会触发 `CONNECTION_CLOSED` 事件

---

#### 3.2.5 write

```cpp
ssize_t write(const uint8_t* data, size_t len, bool fin)
```

**功能**: 向流写入数据（线程安全）

**参数**:
- `data`: 数据缓冲区指针（可为nullptr当fin=true时）
- `len`: 数据长度（0-65536字节）
- `fin`: 是否为流的最后数据（FIN标志）

**返回值**:
- `>= 0`: 成功写入的字节数
- `-1`: 错误（检查 `getLastError()` 获取详情）

**内部行为**:
1. 验证参数（长度 ≤ 65536）
2. 创建 `WRITE` 命令，拷贝数据
3. 将命令压入线程安全队列
4. 触发事件循环异步唤醒
5. 立即返回（非阻塞）

**示例**:
```cpp
// 发送数据
std::string message = "Hello, QUIC!";
ssize_t written = engine.write(
    reinterpret_cast<const uint8_t*>(message.data()),
    message.size(),
    false  // 未结束
);

if (written < 0) {
    std::cerr << "Write failed: " << engine.getLastError() << std::endl;
}

// 发送FIN（关闭写端）
engine.write(nullptr, 0, true);
```

**最大写入限制**:
- 单次写入最大 65536 字节（64KB）
- 超过限制需要分块发送

**注意事项**:
- 使用内部默认流ID（构造时初始化为4）
- 线程安全，可从任意线程调用
- 数据会被拷贝，`write()` 返回后可立即释放缓冲区

---

#### 3.2.6 read

```cpp
ssize_t read(uint8_t* buf, size_t buf_len, bool& fin)
```

**功能**: 从流读取数据（线程安全）

**参数**:
- `buf`: 接收缓冲区
- `buf_len`: 缓冲区大小
- `fin`: 输出参数，是否收到FIN标志

**返回值**:
- `> 0`: 读取的字节数
- `0`: 当前无数据可读
- `-1`: 致命错误

**内部行为**:
1. 获取流缓冲区（自动创建）
2. 加锁访问缓冲区
3. 从缓冲区拷贝数据到用户buffer
4. 更新读偏移量
5. 检查FIN状态

**示例**:
```cpp
uint8_t buf[65536];
bool fin = false;

// 轮询读取
while (!should_stop) {
    ssize_t len = engine.read(buf, sizeof(buf), fin);

    if (len > 0) {
        // 处理接收到的数据
        process_data(buf, len);
        std::cout << "Received " << len << " bytes" << std::endl;
    } else if (len == 0) {
        // 无数据，继续轮询
    } else {
        // 错误
        std::cerr << "Read error" << std::endl;
        break;
    }

    if (fin) {
        std::cout << "Stream finished (FIN received)" << std::endl;
        break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

**注意事项**:
- 使用内部默认流ID
- 线程安全，可从任意线程调用
- 返回0不表示错误，只是暂无数据
- `fin=true` 表示对端已关闭写端，所有数据已接收完毕

---

#### 3.2.7 状态查询接口

```cpp
bool isConnected() const
```
**功能**: 检查连接是否已建立
**返回**: `true` 表示已连接

---

```cpp
bool isRunning() const
```
**功能**: 检查事件循环是否正在运行
**返回**: `true` 表示正在运行

---

```cpp
EngineStats getStats() const
```
**功能**: 获取连接统计信息

**返回结构**:
```cpp
struct EngineStats {
    size_t packets_sent;       // 发送的数据包数
    size_t packets_received;   // 接收的数据包数
    size_t bytes_sent;         // 发送的字节数
    size_t bytes_received;     // 接收的字节数
    size_t packets_lost;       // 丢失的数据包数
    uint64_t rtt_ns;           // 往返时延（纳秒）
    uint64_t cwnd;             // 拥塞窗口（字节）
};
```

**示例**:
```cpp
EngineStats stats = engine.getStats();
std::cout << "RTT: " << (stats.rtt_ns / 1000000.0) << " ms\n"
          << "CWND: " << stats.cwnd << " bytes\n"
          << "Packets lost: " << stats.packets_lost << std::endl;
```

---

```cpp
std::string getLastError() const
```
**功能**: 获取最后一次错误消息
**返回**: 错误描述字符串

---

```cpp
std::string getScid() const
```
**功能**: 获取源连接ID（Source Connection ID）
**返回**: 8字符十六进制字符串
**示例**: `"a3f2c8e1"`

---

## 4. 事件系统

### 4.1 事件类型

```cpp
enum class EngineEvent {
    CONNECTED,           // 连接建立成功
    CONNECTION_CLOSED,   // 连接已关闭
    STREAM_READABLE,     // 流有数据可读（轮询模式下不使用）
    STREAM_WRITABLE,     // 流可写（保留，未使用）
    DATAGRAM_RECEIVED,   // 收到不可靠数据报（保留，未使用）
    ERROR,               // 发生错误
};
```

### 4.2 事件数据

```cpp
struct EventData {
    EventDataType type;  // NONE, STRING, UINT64
    std::string str_val; // 字符串值
    uint64_t uint_val;   // 整数值
};
```

### 4.3 事件触发时机与携带数据

| 事件 | 触发时机 | EventData内容 | 典型处理 |
|------|---------|--------------|----------|
| **CONNECTED** | QUIC握手完成 | `type=STRING`<br>`str_val="已连接"` | 标记连接就绪<br>启动数据传输线程 |
| **CONNECTION_CLOSED** | 连接关闭<br>（正常/异常） | `type=NONE` | 停止数据传输<br>打印统计信息<br>清理资源 |
| **STREAM_READABLE** | 流缓冲区有新数据<br>（事件驱动模式） | `type=UINT64`<br>`uint_val=stream_id` | 调用 `read()` 读取数据 |
| **ERROR** | 连接/引擎错误 | `type=STRING`<br>`str_val=错误描述` | 记录错误<br>调用 `shutdown()` |

### 4.4 事件处理模式

#### 轮询模式（Polling Mode）- 推荐用于移动端
```cpp
// client.cpp 示例
void onEvent(QuicheEngine* engine, EngineEvent event,
             const EventData& data, void* user_data) {
    switch (event) {
        case EngineEvent::CONNECTED:
            connection_ready = true;  // 标记连接就绪
            break;
        case EngineEvent::CONNECTION_CLOSED:
            should_stop = true;       // 停止轮询
            break;
        case EngineEvent::ERROR:
            std::cerr << "Error: " << engine->getLastError() << std::endl;
            should_stop = true;
            break;
        default:
            // 忽略 STREAM_READABLE（轮询模式不需要）
            break;
    }
}

// 数据接收线程（轮询）
void receiverThread() {
    while (!should_stop) {
        uint8_t buf[65536];
        bool fin = false;
        ssize_t len = engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            process_data(buf, len);
        }

        if (fin) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

#### 事件驱动模式（Event-Driven Mode）
```cpp
void onEvent(QuicheEngine* engine, EngineEvent event,
             const EventData& data, void* user_data) {
    if (event == EngineEvent::STREAM_READABLE) {
        uint64_t stream_id = data.uint_val;

        uint8_t buf[65536];
        bool fin = false;
        ssize_t len = engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            process_data(buf, len);
        }
    }
}
```

---

## 5. 使用示例

### 5.1 基本客户端（来自 quic-demo/src/client.cpp）

```cpp
#include <quiche_engine.h>
#include <iostream>
#include <thread>
#include <atomic>

using namespace quiche;

// 全局状态
static std::atomic<bool> connection_ready(false);
static std::atomic<bool> should_stop(false);

// 事件回调
void onEvent(QuicheEngine* engine, EngineEvent event,
             const EventData& data, void* user_data) {
    switch (event) {
        case EngineEvent::CONNECTED:
            std::cout << "✓ Connected: " << data.str_val << std::endl;
            connection_ready = true;
            break;

        case EngineEvent::CONNECTION_CLOSED:
            std::cout << "✓ Connection closed" << std::endl;

            // 打印统计
            EngineStats stats = engine->getStats();
            std::cout << "Packets sent: " << stats.packets_sent << "\n"
                      << "Packets recv: " << stats.packets_received << "\n"
                      << "RTT: " << (stats.rtt_ns / 1000000.0) << " ms" << std::endl;

            should_stop = true;
            break;

        case EngineEvent::ERROR:
            std::cerr << "✗ Error: " << engine->getLastError() << std::endl;
            should_stop = true;
            break;

        default:
            break;
    }
}

// 数据接收线程
void receiverThread(QuicheEngine* engine) {
    // 等待连接就绪
    while (!connection_ready && !should_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    uint8_t buf[65536];
    bool fin = false;

    // 轮询读取
    while (!should_stop) {
        ssize_t len = engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            std::cout << "Received " << len << " bytes" << std::endl;
        }

        if (fin) {
            std::cout << "Stream finished" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 数据发送线程
void senderThread(QuicheEngine* engine) {
    // 等待连接就绪
    while (!connection_ready && !should_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 发送数据
    std::string message = "Hello, QUIC Server!";
    ssize_t written = engine->write(
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size(),
        false
    );

    if (written > 0) {
        std::cout << "Sent " << written << " bytes" << std::endl;
    }

    // 发送FIN
    engine->write(nullptr, 0, true);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    should_stop = true;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    // 1. 创建配置
    ConfigMap config;
    config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;
    config[ConfigKey::INITIAL_MAX_DATA] = 100000000;

    // 2. 创建引擎
    QuicheEngine engine(argv[1], argv[2], config);

    // 3. 设置回调
    if (!engine.setEventCallback(onEvent, nullptr)) {
        std::cerr << "Failed to set callback" << std::endl;
        return 1;
    }

    // 4. 启动引擎
    if (!engine.start()) {
        std::cerr << "Failed to start: " << engine.getLastError() << std::endl;
        return 1;
    }

    std::cout << "Engine started, SCID: " << engine.getScid() << std::endl;

    // 5. 启动工作线程
    std::thread recv_thread(receiverThread, &engine);
    std::thread send_thread(senderThread, &engine);

    // 6. 等待完成
    recv_thread.join();
    send_thread.join();

    // 7. 关闭连接
    engine.shutdown(0, "Completed");

    std::cout << "Done" << std::endl;
    return 0;
}
```

### 5.2 完整工作流程

```
1. 应用启动
   ├─ 创建 ConfigMap
   ├─ 实例化 QuicheEngine(host, port, config)
   └─ 生成SCID，分配资源

2. 设置回调
   └─ setEventCallback(callback, user_data)

3. 启动引擎（非阻塞）
   ├─ start()
   ├─ 创建socket
   ├─ 启动QUIC握手
   └─ 启动事件循环线程
       ↓
   [事件循环运行]
       ├─ Socket I/O (recvmmsg/sendmmsg)
       ├─ 处理QUIC协议
       ├─ 触发回调
       └─ 管理定时器

4. 连接建立
   └─ 触发 CONNECTED 事件
       ↓
   [应用开始数据传输]

5. 数据发送
   ├─ 应用线程调用 write(data, len, fin)
   ├─ 命令入队 → CommandQueue
   ├─ 唤醒事件循环
   └─ 事件循环发送数据

6. 数据接收（轮询模式）
   ├─ 应用线程循环调用 read(buf, len, fin)
   ├─ 从 StreamReadBuffer 读取
   └─ 处理接收数据

7. 关闭连接
   ├─ shutdown(error, reason)
   ├─ 发送CONNECTION_CLOSE帧
   ├─ 触发 CONNECTION_CLOSED 事件
   ├─ 停止事件循环
   └─ 清理资源

8. 应用退出
   └─ QuicheEngine析构
       └─ 自动调用shutdown()（如未手动调用）
```

---

## 6. 最佳实践

### 6.1 配置建议

#### 低延迟场景（游戏、实时通信）
```cpp
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 10000;  // 10秒
config[ConfigKey::INITIAL_MAX_DATA] = 10000000;  // 10MB
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = 1000000;  // 1MB
config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = false;  // 允许迁移
```

#### 大文件传输（下载、上传）
```cpp
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 60000;  // 60秒
config[ConfigKey::INITIAL_MAX_DATA] = 500000000;  // 500MB
config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = 100000000;  // 100MB
config[ConfigKey::INITIAL_MAX_STREAMS_BIDI] = 10;  // 限制流数量
```

#### 移动网络（不稳定网络）
```cpp
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;  // 30秒
config[ConfigKey::MAX_UDP_PAYLOAD_SIZE] = 1200;  // 降低MTU
config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = false;  // 支持网络切换
```

### 6.2 线程安全

✅ **线程安全接口**（可从任意线程调用）:
- `write()`
- `read()`
- `isConnected()`
- `isRunning()`
- `getStats()`
- `getLastError()`
- `getScid()`

⚠️ **非线程安全接口**（必须从主线程调用）:
- `setEventCallback()`
- `start()`
- `shutdown()`

### 6.3 错误处理

```cpp
// 1. 检查 start() 返回值
if (!engine.start()) {
    std::cerr << "Start failed: " << engine.getLastError() << std::endl;
    return 1;
}

// 2. 检查 write() 返回值
ssize_t written = engine.write(data, len, false);
if (written < 0) {
    std::cerr << "Write failed: " << engine.getLastError() << std::endl;
}

// 3. 监听 ERROR 事件
void onEvent(...) {
    if (event == EngineEvent::ERROR) {
        std::string error = engine->getLastError();
        log_error(error);
        engine->shutdown(1, error);
    }
}
```

### 6.4 资源管理

```cpp
// RAII - 自动资源管理
{
    QuicheEngine engine(host, port, config);
    // 使用引擎...
}  // 析构函数自动调用 shutdown() 和清理资源

// 手动管理（推荐）
QuicheEngine engine(host, port, config);
try {
    // 使用引擎...
    engine.shutdown(0, "Normal close");
} catch (const std::exception& e) {
    engine.shutdown(1, e.what());
}
```

### 6.5 性能优化

#### 批量发送
```cpp
// ❌ 不好：频繁小包发送
for (const auto& msg : messages) {
    engine.write(msg.data(), msg.size(), false);
}

// ✅ 好：合并后发送
std::vector<uint8_t> batch;
for (const auto& msg : messages) {
    batch.insert(batch.end(), msg.begin(), msg.end());
}
engine.write(batch.data(), batch.size(), false);
```

#### 读取缓冲区大小
```cpp
// 推荐使用 64KB 缓冲区
uint8_t buf[65536];  // 匹配最大写入限制
```

#### 轮询间隔
```cpp
// 低延迟场景
std::this_thread::sleep_for(std::chrono::milliseconds(1));

// 一般场景
std::this_thread::sleep_for(std::chrono::milliseconds(10));

// 省电场景
std::this_thread::sleep_for(std::chrono::milliseconds(50));
```

### 6.6 调试技巧

#### 启用调试日志
```cpp
config[ConfigKey::ENABLE_DEBUG_LOG] = true;
```

#### 监控连接状态
```cpp
void monitorThread(QuicheEngine* engine) {
    while (engine->isRunning()) {
        if (engine->isConnected()) {
            EngineStats stats = engine->getStats();
            std::cout << "[" << std::time(nullptr) << "] "
                      << "RTT=" << (stats.rtt_ns / 1000000.0) << "ms, "
                      << "CWND=" << stats.cwnd << "bytes, "
                      << "Loss=" << stats.packets_lost << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

---

## 附录 A: 平台差异

### I/O 实现

| 平台 | 接收 | 发送 | 批量操作 |
|------|------|------|----------|
| **Linux/Android** | `recvmmsg()` | `sendmmsg()` | 是（最多32包/次） |
| **macOS/iOS** | `recvmsg()` 循环 | `sendmsg()` 循环 | 否 |

### 内存占用

| 组件 | Linux/Android | macOS/iOS |
|------|--------------|-----------|
| 发送缓冲区 | 32 × 1350B = 43KB | 1 × 1350B = 1.4KB |
| 接收缓冲区 | 32 × 2048B = 64KB | 1 × 2048B = 2KB |
| **总计** | ~110KB | ~3.4KB |

---

## 附录 B: 常见问题

### Q1: write() 返回的字节数和传入的 len 不一致？
**A**: `write()` 总是返回 `len` 或 `-1`。数据被完整拷贝到命令队列，实际网络发送由事件循环异步完成。

### Q2: read() 返回 0 是错误吗？
**A**: 不是。返回 0 表示当前无数据可读，应继续轮询或等待 `STREAM_READABLE` 事件。

### Q3: 如何知道数据发送完成？
**A**: QUIC 是流式协议，无"发送完成"的概念。数据通过 `write()` 提交后会被可靠传输。可通过 `getStats().bytes_sent` 监控发送字节数。

### Q4: 支持多流吗？
**A**: 当前实现使用固定的内部流ID（默认4）。多流支持需要扩展API。

### Q5: 如何处理网络切换（WiFi ↔ 4G）？
**A**: 设置 `DISABLE_ACTIVE_MIGRATION = false` 启用连接迁移。QUIC会自动处理IP地址变化。

### Q6: 可以在Android/iOS使用吗？
**A**: 是的。通过 `build_mobile_libs.sh` 构建平台特定的静态库（libquiche_engine.a）。

---

## 附录 C: 版本信息

- **Quiche Engine API**: v1.0
- **Quiche 核心库**: v0.24.6
- **QUIC 版本**: RFC 9000 (QUIC v1)
- **TLS 版本**: TLS 1.3
- **支持平台**: Linux, macOS, iOS, Android
- **最低编译器**: C++11

---

## 附录 D: 许可证

Copyright (C) 2025, Cloudflare, Inc.
All rights reserved.

---

**文档生成时间**: 2025-01-06
**最后更新**: 2025-01-06
