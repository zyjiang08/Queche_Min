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
┌──────────────────────────────────────────────────────────────────┐
│                     应用层 (Application)                          │
│                  quic-demo/src/client.cpp                        │
│                  quic-demo/src/server.c                          │
└───────────────────────────┬──────────────────────────────────────┘
                            │ 使用
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│                 Quiche Engine 公共API层                           │
│           quiche/engine/include/quiche_engine.h                  │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                QuicheEngine 类                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │ 构造/析构  配置  事件回调  连接管理  数据读写  状态查询   │  │ │
│  │  │ open() setEventCallback() connect() close()          │  │ │
│  │  │ write() read() getStats()                            │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
└───────────────────────────┬──────────────────────────────────────┘
                            │ PIMPL模式
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│               Quiche Engine 实现层                                │
│       quiche/engine/src/quiche_engine_impl.{h,cpp}               │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │             QuicheEngineImpl 类                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │ • 事件循环线程 (libev)                                 │  │ │
│  │  │ • 命令队列 (线程安全)                                   │  │ │
│  │  │ • 流缓冲区管理                                         │  │ │
│  │  │ • Socket I/O (批量/单包)                              │  │ │
│  │  │ • 同步连接控制 (condition_variable)                    │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
└───────────────────────────┬──────────────────────────────────────┘
                            │ 调用
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│                   Quiche 核心库                                   │
│                quiche/include/quiche.h                           │
│                quiche/src/lib.rs                                 │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ • QUIC协议实现 (RFC 9000)                                    │ │
│  │ • TLS 1.3集成 (BoringSSL)                                   │ │
│  │ • 流多路复用                                                 │ │
│  │ • 拥塞控制 (CUBIC/BBR/Reno)                                  │ │
│  │ • 丢包恢复                                                   │ │
│  │ • 连接迁移                                                   │ │
│  └────────────────────────────────────────────────────────────┘ │
└───────────────────────────┬──────────────────────────────────────┘
                            │ 依赖
                            ↓
┌──────────────────────────────────────────────────────────────────┐
│                      底层依赖                                     │
│                                                                  │
│  ┌────────────┐    ┌─────────────┐    ┌─────────────┐            │
│  │  libev     │    │ BoringSSL   │    │ std::thread │            │
│  │ (事件循环)  │    │ (TLS/加密)   │    │ (C++11线程)  │            │
│  └────────────┘    └─────────────┘    └─────────────┘            │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 线程模型

```
应用线程                        事件循环线程
(Main Thread)                  (Event Loop Thread)
     │                                │
     │  1. 创建QuicheEngine()          │
     ├──────────────────────────────→ │
     │                                │
     │  2. open(config)               │
     ├──────────────────────────────→ │
     │                                │
     │  3. setEventCallback()         │
     ├──────────────────────────────→ │
     │                                │
     │  4. connect(host, port, timeout)│
     ├──────────────────────────────→ │ 启动事件循环
     │                                │ ev_run()
     │  阻塞等待连接结果               ├──────────┐
     │  (condition_variable)          │          │
     │                                │  • Socket I/O
     │←─────────────[通知]─────────────          │
     │  连接成功/失败                  │  • quiche握手
     │  返回CID或空字符串              │  • 定时器
     │                                │          │
     │  5. write(data)                │          │
     ├───→ [命令队列] ─────────────────→          │
     │                                │  • quiche处理
     │  6. read(buf) ←─ [流缓冲区] ←───          │
     │                                │  • 事件回调
     ↓                                ↓          ↓
数据发送线程                      事件回调
(Send Thread)                   (Callbacks)
     │                                │
     │  write()                       │  CONNECTED
     ├──────────────────────────────→ │  STREAM_READABLE
     │                                │  CONNECTION_CLOSED
                                      │  ERROR
数据接收线程                            │
(Recv Thread)                         │
     │                                │
     │  read()                        │
     │←────────────────────────────── ┘
     │                                │
     │  7. close(error, reason)       │
     ├──────────────────────────────→ │
     │                                │ 发送CLOSE帧
     │                                │ 停止事件循环
     │                                │ 清理资源
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
        QuicheEngine();  // 无参构造函数
        ~QuicheEngine();

        // 移动语义（支持）
        QuicheEngine(QuicheEngine&& other) noexcept;
        QuicheEngine& operator=(QuicheEngine&& other) noexcept;

        // 拷贝语义（禁用）
        QuicheEngine(const QuicheEngine&) = delete;
        QuicheEngine& operator=(const QuicheEngine&) = delete;

        // 核心API
        bool open(const ConfigMap& config);
        bool setEventCallback(EventCallback callback, void* user_data = nullptr);
        std::string connect(const std::string& host, const std::string& port,
                           uint64_t timeout_ms = 5000);
        void close(uint64_t app_error = 0, const std::string& reason = "");
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

### 3.2 API 调用顺序

**必须按以下顺序调用**:
```cpp
1. QuicheEngine engine;           // 创建引擎实例
2. engine.open(config);            // 设置配置（可选，使用默认配置则可跳过）
3. engine.setEventCallback(...);  // 设置事件回调（必须）
4. engine.connect(host, port);     // 同步连接（阻塞直到成功或超时）
5. engine.write(...) / read(...);  // 数据传输
6. engine.close(...);              // 关闭连接（可选，析构函数会自动调用）
```

### 3.3 接口详细说明

#### 3.3.1 构造函数

```cpp
QuicheEngine()
```

**功能**: 创建空的QUIC引擎实例

**参数**: 无

**内部行为**:
1. 分配PIMPL实现对象
2. 初始化内部状态标志
3. 分配基础资源（互斥锁等）
4. **不**创建连接或启动事件循环

**示例**:
```cpp
// 默认构造
QuicheEngine engine;
```

**注意事项**:
- 构造后必须调用 `open()` 设置配置（或使用默认配置）
- 构造后必须调用 `setEventCallback()` 设置回调
- 构造后必须调用 `connect()` 建立连接
- 轻量级操作，不会阻塞

---

#### 3.3.2 open

```cpp
bool open(const ConfigMap& config)
```

**功能**: 设置QUIC配置参数

**参数**:
- `config`: 配置参数映射

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

**返回值**:
- `true`: 设置成功
- `false`: 设置失败

**示例**:
```cpp
QuicheEngine engine;

// 默认配置（可以跳过open调用）
engine.open(ConfigMap());

// 自定义配置
ConfigMap config;
config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;  // 30秒
config[ConfigKey::INITIAL_MAX_DATA] = 100000000;  // 100MB
config[ConfigKey::ENABLE_DEBUG_LOG] = true;
engine.open(config);
```

**注意事项**:
- 必须在 `connect()` 之前调用
- 可以在 `setEventCallback()` 之前或之后调用
- 配置只在下次 `connect()` 时生效
- `close()` 后配置保留，可直接重新 `connect()`

---

#### 3.3.3 setEventCallback

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

**注意事项**:
- 必须在 `connect()` 之前调用
- 可以在 `open()` 之前或之后调用
- 回调在事件循环线程中执行，应避免耗时操作
- `close()` 后回调保留，可直接重新 `connect()`

---

#### 3.3.4 connect

```cpp
std::string connect(const std::string& host, const std::string& port,
                   uint64_t timeout_ms = 5000)
```

**功能**: 同步连接到服务器（阻塞直到连接成功或超时）

**前置条件**:
- 必须先调用 `open()` 设置配置（或使用默认配置）
- 必须先调用 `setEventCallback()` 设置回调

**参数**:
- `host`: 远程主机名或IP地址
- `port`: 远程端口号
- `timeout_ms`: 超时时间（毫秒），默认5000ms

**返回值**:
- 成功: 8字符十六进制连接ID（SCID），如 `"a3f2c8e1"`
- 失败: 空字符串 `""`（调用 `getLastError()` 获取详情）

**内部行为**:
1. 检查前置条件（`open()` 和 `setEventCallback()` 是否已调用）
2. 生成随机8字符SCID
3. 创建UDP socket
4. 初始化QUIC连接并发送握手包
5. 启动后台事件循环线程
6. **阻塞等待**连接建立或超时（使用 `std::condition_variable`）
7. 返回结果

**示例**:
```cpp
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(onEvent, nullptr);

std::string cid = engine.connect("127.0.0.1", "4433", 10000);
if (cid.empty()) {
    std::cerr << "Connection failed: " << engine.getLastError() << std::endl;
    return 1;
}
std::cout << "Connected! Connection ID: " << cid << std::endl;
```

**错误处理**:
```cpp
std::string cid = engine.connect(host, port, timeout_ms);
if (cid.empty()) {
    std::string error = engine.getLastError();
    if (error.find("timeout") != std::string::npos) {
        // 连接超时
    } else if (error.find("Must call open()") != std::string::npos) {
        // 忘记调用 open()
    } else if (error.find("Must call setEventCallback()") != std::string::npos) {
        // 忘记调用 setEventCallback()
    } else {
        // 其他网络错误
    }
}
```

**注意事项**:
- **同步阻塞**调用，在连接成功/失败前不会返回
- 如果已连接，返回现有连接ID
- 连接成功后会触发 `CONNECTED` 事件
- 超时或失败不会抛出异常，通过返回值判断

---

#### 3.3.5 close

```cpp
void close(uint64_t app_error = 0, const std::string& reason = "")
```

**功能**: 优雅关闭连接（阻塞，等待清理完成）

**参数**:
- `app_error`: 应用层错误码（默认0表示正常关闭）
- `reason`: 关闭原因字符串（可选）

**内部行为**:
1. 发送QUIC连接关闭帧
2. 停止事件循环线程
3. 清理网络资源（socket、quiche对象）
4. **保留**配置和回调（允许重新连接）
5. 等待线程完全停止

**示例**:
```cpp
// 正常关闭
engine.close();

// 带错误码和原因
engine.close(1001, "Application timeout");

// 关闭后可以重新连接（无需重新设置配置和回调）
std::string cid = engine.connect(host, port);
```

**与析构函数的区别**:
```cpp
// 手动关闭（保留配置和回调，可重连）
engine.close();
// engine 仍然有效，可以再次 connect()

// 析构函数（完全销毁）
{
    QuicheEngine engine;
    // ...
}  // 自动调用析构 → close() → 销毁对象
```

**注意事项**:
- **阻塞**调用，等待事件循环完全停止
- 析构函数会自动调用 `close()`，手动调用是可选的
- 关闭后会触发 `CONNECTION_CLOSED` 事件
- 配置和回调被保留，支持重新 `connect()`

---

#### 3.3.6 write

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

#### 3.3.7 read

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

#### 3.3.8 状态查询接口

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
| **CONNECTED** | QUIC握手完成 | `type=STRING`<br>`str_val="hq-interop"` | 标记连接就绪<br>启动数据传输线程<br>**注意**: connect()已返回，此事件用于异步通知 |
| **CONNECTION_CLOSED** | 连接关闭<br>（正常/异常） | `type=NONE` | 停止数据传输<br>打印统计信息<br>清理资源 |
| **STREAM_READABLE** | 流缓冲区有新数据<br>（事件驱动模式） | `type=UINT64`<br>`uint_val=stream_id` | 调用 `read()` 读取数据 |
| **ERROR** | 连接/引擎错误 | `type=STRING`<br>`str_val=错误描述` | 记录错误<br>调用 `close()` |

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

### 5.1 基本客户端（新API版本）

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

    // 1. 创建引擎（无参构造）
    QuicheEngine engine;

    // 2. 设置配置（可选）
    ConfigMap config;
    config[ConfigKey::MAX_IDLE_TIMEOUT] = 30000;
    config[ConfigKey::INITIAL_MAX_DATA] = 100000000;

    if (!engine.open(config)) {
        std::cerr << "Failed to open engine" << std::endl;
        return 1;
    }

    // 3. 设置回调
    if (!engine.setEventCallback(onEvent, nullptr)) {
        std::cerr << "Failed to set callback" << std::endl;
        return 1;
    }

    // 4. 同步连接（阻塞）
    std::cout << "Connecting to " << argv[1] << ":" << argv[2] << "..." << std::endl;
    std::string cid = engine.connect(argv[1], argv[2], 10000);

    if (cid.empty()) {
        std::cerr << "Connection failed: " << engine.getLastError() << std::endl;
        return 1;
    }

    std::cout << "✓ Connected! Connection ID: " << cid << std::endl;

    // 5. 启动工作线程
    std::thread recv_thread(receiverThread, &engine);
    std::thread send_thread(senderThread, &engine);

    // 6. 等待完成
    recv_thread.join();
    send_thread.join();

    // 7. 关闭连接
    engine.close(0, "Completed");

    std::cout << "Done" << std::endl;
    return 0;
}
```

### 5.2 重连示例

```cpp
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(onEvent, nullptr);

// 第一次连接
std::string cid = engine.connect("server1.com", "4433");
if (!cid.empty()) {
    // 使用连接...
    engine.close();
}

// 重连到另一台服务器（无需重新设置配置和回调）
cid = engine.connect("server2.com", "4433");
if (!cid.empty()) {
    // 使用连接...
    engine.close();
}
```

### 5.3 完整工作流程

```
1. 应用启动
   ├─ 实例化 QuicheEngine()
   └─ 分配基础资源

2. 设置配置（可选）
   └─ open(config)

3. 设置回调
   └─ setEventCallback(callback, user_data)

4. 同步连接（阻塞）
   ├─ connect(host, port, timeout)
   ├─ 创建socket
   ├─ 启动QUIC握手
   ├─ 启动事件循环线程
   ├─ 阻塞等待连接结果
   └─ 返回连接ID或空字符串
       ↓
   [连接成功]
       ├─ 触发 CONNECTED 事件
       └─ 事件循环持续运行
           ├─ Socket I/O (recvmmsg/sendmmsg)
           ├─ 处理QUIC协议
           ├─ 触发回调
           └─ 管理定时器

5. 数据传输
   ├─ 应用线程调用 write(data, len, fin)
   ├─ 命令入队 → CommandQueue
   ├─ 唤醒事件循环
   ├─ 事件循环发送数据
   │
   ├─ 应用线程循环调用 read(buf, len, fin)
   ├─ 从 StreamReadBuffer 读取
   └─ 处理接收数据

6. 关闭连接
   ├─ close(error, reason)
   ├─ 发送CONNECTION_CLOSE帧
   ├─ 触发 CONNECTION_CLOSED 事件
   ├─ 停止事件循环
   ├─ 清理网络资源
   └─ 保留配置和回调（支持重连）

7. 重连或退出
   ├─ 选项A: 重连
   │   └─ connect(new_host, new_port)
   │
   └─ 选项B: 退出
       └─ QuicheEngine析构
           └─ 自动调用close()（如未手动调用）
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
- `open()`
- `setEventCallback()`
- `connect()` （阻塞，建议从主线程调用）
- `close()` （阻塞，建议从主线程调用）

### 6.3 错误处理

```cpp
// 1. 检查 open() 返回值
if (!engine.open(config)) {
    std::cerr << "Open failed: " << engine.getLastError() << std::endl;
    return 1;
}

// 2. 检查 connect() 返回值
std::string cid = engine.connect(host, port, 10000);
if (cid.empty()) {
    std::cerr << "Connect failed: " << engine.getLastError() << std::endl;
    return 1;
}

// 3. 检查 write() 返回值
ssize_t written = engine.write(data, len, false);
if (written < 0) {
    std::cerr << "Write failed: " << engine.getLastError() << std::endl;
}

// 4. 监听 ERROR 事件
void onEvent(...) {
    if (event == EngineEvent::ERROR) {
        std::string error = engine->getLastError();
        log_error(error);
        engine->close(1, error);
    }
}
```

### 6.4 资源管理

```cpp
// RAII - 自动资源管理
{
    QuicheEngine engine;
    engine.open(config);
    engine.setEventCallback(onEvent, nullptr);

    std::string cid = engine.connect(host, port);
    if (!cid.empty()) {
        // 使用引擎...
    }
}  // 析构函数自动调用 close() 和清理资源

// 手动管理（推荐）
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(onEvent, nullptr);

try {
    std::string cid = engine.connect(host, port);
    if (!cid.empty()) {
        // 使用引擎...
        engine.close(0, "Normal close");
    }
} catch (const std::exception& e) {
    engine.close(1, e.what());
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

### 6.7 连接超时处理

```cpp
// 设置合理的超时时间
std::string cid = engine.connect(host, port, 10000);  // 10秒超时

if (cid.empty()) {
    std::string error = engine.getLastError();

    if (error.find("timeout") != std::string::npos) {
        // 连接超时，可能需要：
        // 1. 检查网络连接
        // 2. 检查服务器是否可达
        // 3. 增加超时时间重试

        std::cerr << "Connection timeout, retrying with longer timeout..." << std::endl;
        cid = engine.connect(host, port, 30000);  // 30秒超时
    }
}
```

### 6.8 重连策略

```cpp
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(onEvent, nullptr);

int retry_count = 0;
const int max_retries = 3;
const uint64_t base_timeout = 5000;

while (retry_count < max_retries) {
    uint64_t timeout = base_timeout * (1 << retry_count);  // 指数退避
    std::string cid = engine.connect(host, port, timeout);

    if (!cid.empty()) {
        // 连接成功
        break;
    }

    retry_count++;
    std::cerr << "Connection attempt " << retry_count << " failed, ";

    if (retry_count < max_retries) {
        std::cerr << "retrying..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
        std::cerr << "max retries reached" << std::endl;
        return 1;
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
**A**: 是的。通过 `quiche_engine_all.sh` 构建平台特定的静态库（libquiche_engine.a）。

### Q7: connect() 和 CONNECTED 事件的关系？
**A**: `connect()` 是同步调用，返回时连接已建立。`CONNECTED` 事件是异步通知，在 `connect()` 内部完成连接后触发，主要用于事件驱动编程模式。

### Q8: 为什么 connect() 需要先调用 open() 和 setEventCallback()？
**A**:
- `open()` 设置QUIC配置参数，必须在连接前确定
- `setEventCallback()` 设置事件回调，连接过程中会触发事件（如ERROR）
- 这种设计确保所有必要的配置和回调在连接建立前就已准备好

### Q9: close() 和析构函数有什么区别？
**A**:
- `close()`: 关闭连接，保留配置和回调，可重新 `connect()`
- 析构函数: 调用 `close()` 并销毁所有资源，对象不可再使用

### Q10: connect() 阻塞期间可以取消吗？
**A**: 当前实现不支持。如需取消功能，可以在单独的线程中调用 `connect()`，并使用超时控制。

---

## 附录 C: API 变更历史

### v2.0 (2025-01-08) - 当前版本
**重大变更**:
- 构造函数从 `QuicheEngine(host, port, config)` 改为 `QuicheEngine()`
- 删除 `start()` 方法
- 删除 `shutdown()` 方法
- 新增 `open(config)` 方法
- 新增 `connect(host, port, timeout)` 同步连接方法
- 新增 `close(app_error, reason)` 方法，支持重连

**优势**:
- 更清晰的API调用顺序
- 同步连接简化错误处理
- 支持连接复用（关闭后可重连）
- 更好的状态管理

**迁移指南**:
```cpp
// v1.0 旧代码
QuicheEngine engine(host, port, config);
engine.setEventCallback(onEvent, nullptr);
if (!engine.start()) {
    // 处理错误
}
// 使用连接...
engine.shutdown();

// v2.0 新代码
QuicheEngine engine;
engine.open(config);
engine.setEventCallback(onEvent, nullptr);
std::string cid = engine.connect(host, port);
if (cid.empty()) {
    // 处理错误
}
// 使用连接...
engine.close();
```

### v1.0 (2025-01-06)
- 初始版本
- 构造函数接受 host, port, config 参数
- 使用 `start()` 启动异步连接
- 使用 `shutdown()` 关闭连接

---

## 附录 D: 版本信息

- **Quiche Engine API**: v2.0
- **Quiche 核心库**: v0.24.6
- **QUIC 版本**: RFC 9000 (QUIC v1)
- **TLS 版本**: TLS 1.3
- **支持平台**: Linux, macOS, iOS, Android
- **最低编译器**: C++11

---

**文档生成时间**: 2025-01-08
**最后更新**: 2025-01-08 (API v2.0 重构)
