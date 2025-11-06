# Mobile Integration Examples

## 📱 简介

本文档提供 iOS 和 Android 平台集成 `libquiche_engine` 的实际代码示例。

## 🍎 iOS 集成

### 1. 项目配置

#### 步骤 A: 添加库文件

```
YourApp/
├── YourApp.xcodeproj
├── Frameworks/
│   └── libquiche_engine.a        # 从 mobile_libs/ios/arm64/ 复制
└── Headers/
    └── quiche_engine.h           # 从 mobile_libs/ios/include/ 复制
```

#### 步骤 B: Xcode 设置

1. **添加库**：
   - Target -> General -> Frameworks, Libraries, and Embedded Content
   - 点击 "+" -> Add Other -> Add Files
   - 选择 `libquiche_engine.a`

2. **配置 Build Settings**：
   ```
   Library Search Paths: $(PROJECT_DIR)/Frameworks
   Header Search Paths: $(PROJECT_DIR)/Headers
   Other Linker Flags: -lc++ -lresolv
   ```

3. **创建 Bridging Header** (Swift 项目)：
   - File -> New -> File -> Header File
   - 命名为 `YourApp-Bridging-Header.h`
   - 在 Build Settings 中设置：
     ```
     Objective-C Bridging Header: YourApp/YourApp-Bridging-Header.h
     ```

### 2. Bridging Header

**YourApp-Bridging-Header.h**:
```objc
//
//  YourApp-Bridging-Header.h
//

#ifndef YourApp_Bridging_Header_h
#define YourApp_Bridging_Header_h

#import "quiche_engine.h"

#endif
```

### 3. Swift 包装类

**QuicheManager.swift**:
```swift
import Foundation

class QuicheManager {
    private var engine: UnsafeMutablePointer<quiche.QuicheEngine>?
    private var eventHandler: ((EngineEvent) -> Void)?

    // 连接到服务器
    func connect(host: String, port: Int, completion: @escaping (Bool, String?) -> Void) {
        // 创建配置
        var config = ConfigMap()

        // 设置 QUIC 参数
        config[ConfigKey(rawValue: 0)] = ConfigValue(uint64Value: 30000)  // MAX_IDLE_TIMEOUT
        config[ConfigKey(rawValue: 1)] = ConfigValue(uint64Value: 1350)   // MAX_UDP_PAYLOAD_SIZE
        config[ConfigKey(rawValue: 2)] = ConfigValue(uint64Value: 10000000)  // INITIAL_MAX_DATA

        // 创建引擎
        let hostCStr = (host as NSString).utf8String
        let portCStr = (String(port) as NSString).utf8String

        engine = quiche_engine_create(hostCStr, portCStr, &config)

        guard let engine = engine else {
            completion(false, "Failed to create engine")
            return
        }

        // 设置事件回调
        let context = Unmanaged.passUnretained(self).toOpaque()
        quiche_engine_set_event_callback(engine, { engine, event, eventData, userData in
            guard let userData = userData else { return }
            let manager = Unmanaged<QuicheManager>.fromOpaque(userData).takeUnretainedValue()

            switch event.rawValue {
            case 0: // CONNECTED
                print("✓ Connected to server")
                manager.eventHandler?(.connected)

            case 1: // CONNECTION_CLOSED
                print("✓ Connection closed")
                manager.eventHandler?(.closed)

            case 2: // STREAM_READABLE
                print("✓ Stream has data")
                manager.eventHandler?(.readable)

            case 3: // ERROR
                let errorMsg = String(cString: quiche_engine_get_last_error(engine))
                print("✗ Error: \(errorMsg)")
                manager.eventHandler?(.error(errorMsg))

            default:
                break
            }
        }, context)

        // 启动引擎
        let started = quiche_engine_start(engine)
        if started {
            completion(true, nil)
        } else {
            let error = String(cString: quiche_engine_get_last_error(engine))
            completion(false, error)
        }
    }

    // 发送数据
    func send(streamId: UInt64, data: Data, finish: Bool = false) -> Int {
        guard let engine = engine else { return -1 }

        return data.withUnsafeBytes { bufferPtr in
            guard let baseAddress = bufferPtr.baseAddress else { return -1 }
            return Int(quiche_engine_write(engine, streamId, baseAddress, data.count, finish))
        }
    }

    // 接收数据
    func receive(streamId: UInt64) -> (data: Data?, finished: Bool) {
        guard let engine = engine else { return (nil, false) }

        var buffer = [UInt8](repeating: 0, count: 65536)
        var fin: Bool = false

        let len = buffer.withUnsafeMutableBytes { bufferPtr in
            guard let baseAddress = bufferPtr.baseAddress else { return -1 }
            return Int(quiche_engine_read(engine, streamId, baseAddress, buffer.count, &fin))
        }

        if len > 0 {
            return (Data(buffer.prefix(len)), fin)
        } else {
            return (nil, fin)
        }
    }

    // 关闭连接
    func shutdown(error: UInt64 = 0, reason: String = "Normal shutdown") {
        guard let engine = engine else { return }

        let reasonCStr = (reason as NSString).utf8String
        quiche_engine_shutdown(engine, error, reasonCStr)

        self.engine = nil
    }

    // 事件处理
    func onEvent(_ handler: @escaping (EngineEvent) -> Void) {
        self.eventHandler = handler
    }

    deinit {
        if let engine = engine {
            quiche_engine_destroy(engine)
        }
    }
}

// 事件枚举
enum EngineEvent {
    case connected
    case closed
    case readable
    case error(String)
}
```

### 4. 使用示例

**ViewController.swift**:
```swift
import UIKit

class ViewController: UIViewController {
    private let quicheManager = QuicheManager()

    override func viewDidLoad() {
        super.viewDidLoad()

        // 连接到服务器
        connectToServer()
    }

    private func connectToServer() {
        print("Connecting to QUIC server...")

        quicheManager.onEvent { event in
            DispatchQueue.main.async {
                switch event {
                case .connected:
                    print("✓ Connected! Sending request...")
                    self.sendRequest()

                case .readable:
                    self.receiveResponse()

                case .closed:
                    print("✓ Connection closed")

                case .error(let msg):
                    print("✗ Error: \(msg)")
                }
            }
        }

        quicheManager.connect(host: "example.com", port: 443) { success, error in
            if success {
                print("✓ Engine started")
            } else {
                print("✗ Failed to start: \(error ?? "unknown")")
            }
        }
    }

    private func sendRequest() {
        let request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
        if let data = request.data(using: .utf8) {
            let sent = quicheManager.send(streamId: 4, data: data)
            print("✓ Sent \(sent) bytes")
        }
    }

    private func receiveResponse() {
        let (data, finished) = quicheManager.receive(streamId: 4)

        if let data = data, let response = String(data: data, encoding: .utf8) {
            print("✓ Received: \(response.prefix(100))...")
        }

        if finished {
            print("✓ Response complete")
            quicheManager.shutdown()
        }
    }
}
```

## 🤖 Android 集成

### 1. 项目配置

#### 步骤 A: 添加库文件

```
app/
└── src/
    └── main/
        ├── jniLibs/
        │   ├── arm64-v8a/
        │   │   └── libquiche_engine.so
        │   ├── armeabi-v7a/
        │   │   └── libquiche_engine.so
        │   ├── x86/
        │   │   └── libquiche_engine.so
        │   └── x86_64/
        │       └── libquiche_engine.so
        └── cpp/
            ├── include/
            │   └── quiche_engine.h
            └── jni/
                └── quiche_jni.cpp         # JNI 包装层
```

#### 步骤 B: build.gradle 配置

**app/build.gradle**:
```gradle
android {
    compileSdk 34

    defaultConfig {
        applicationId "com.example.quichedemo"
        minSdk 21
        targetSdk 34

        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
        }

        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DANDROID_STL=c++_shared"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path "src/main/cpp/CMakeLists.txt"
        }
    }

    sourceSets {
        main {
            jniLibs.srcDirs = ['src/main/jniLibs']
        }
    }
}
```

#### 步骤 C: CMakeLists.txt

**src/main/cpp/CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.18.1)
project("quichedemo")

# 添加预构建的 libquiche_engine.so
add_library(quiche_engine SHARED IMPORTED)
set_target_properties(quiche_engine PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libquiche_engine.so
)

# 包含头文件
include_directories(${CMAKE_SOURCE_DIR}/include)

# 构建 JNI 包装库
add_library(quichedemo SHARED
    jni/quiche_jni.cpp
)

# 链接
target_link_libraries(quichedemo
    quiche_engine
    log
    android
)
```

### 2. JNI 包装层

**quiche_jni.cpp**:
```cpp
#include <jni.h>
#include <string>
#include <android/log.h>
#include "quiche_engine.h"

#define LOG_TAG "QuicheJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace quiche;

// 存储 Java 回调的全局引用
static JavaVM* g_jvm = nullptr;
static jobject g_callback = nullptr;

// JNI_OnLoad
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

// 事件回调（从 C++ 到 Java）
void eventCallback(QuicheEngine* engine, EngineEvent event,
                   const EventData& data, void* userData) {
    if (!g_jvm || !g_callback) return;

    JNIEnv* env;
    g_jvm->AttachCurrentThread(&env, nullptr);

    // 获取 Java 回调类和方法
    jclass callbackClass = env->GetObjectClass(g_callback);
    jmethodID onEventMethod = env->GetMethodID(callbackClass, "onEvent", "(ILjava/lang/String;)V");

    // 准备事件数据
    jstring jEventData = nullptr;
    if (event == EngineEvent::CONNECTED) {
        try {
            const auto& proto = std::get<std::string>(data);
            jEventData = env->NewStringUTF(proto.c_str());
        } catch (...) {
            jEventData = env->NewStringUTF("");
        }
    } else if (event == EngineEvent::ERROR) {
        std::string error = engine->getLastError();
        jEventData = env->NewStringUTF(error.c_str());
    } else {
        jEventData = env->NewStringUTF("");
    }

    // 调用 Java 回调
    env->CallVoidMethod(g_callback, onEventMethod, static_cast<jint>(event), jEventData);

    env->DeleteLocalRef(jEventData);
    env->DeleteLocalRef(callbackClass);
    g_jvm->DetachCurrentThread();
}

extern "C" {

// 创建引擎
JNIEXPORT jlong JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeCreate(
        JNIEnv* env, jobject thiz, jstring jhost, jstring jport) {

    const char* host = env->GetStringUTFChars(jhost, nullptr);
    const char* port = env->GetStringUTFChars(jport, nullptr);

    // 创建配置
    ConfigMap config;
    config[ConfigKey::MAX_IDLE_TIMEOUT] = static_cast<uint64_t>(30000);
    config[ConfigKey::INITIAL_MAX_DATA] = static_cast<uint64_t>(10000000);

    // 创建引擎
    QuicheEngine* engine = nullptr;
    try {
        engine = new QuicheEngine(host, port, config);
        LOGD("Engine created: %p", engine);
    } catch (const std::exception& e) {
        LOGE("Failed to create engine: %s", e.what());
    }

    env->ReleaseStringUTFChars(jhost, host);
    env->ReleaseStringUTFChars(jport, port);

    return reinterpret_cast<jlong>(engine);
}

// 设置回调
JNIEXPORT void JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeSetCallback(
        JNIEnv* env, jobject thiz, jlong jengine, jobject jcallback) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (!engine) return;

    // 保存全局引用
    if (g_callback) {
        env->DeleteGlobalRef(g_callback);
    }
    g_callback = env->NewGlobalRef(jcallback);

    // 设置回调
    engine->setEventCallback(eventCallback, nullptr);
}

// 启动
JNIEXPORT jboolean JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeStart(
        JNIEnv* env, jobject thiz, jlong jengine) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (!engine) return JNI_FALSE;

    return engine->start() ? JNI_TRUE : JNI_FALSE;
}

// 写入数据
JNIEXPORT jint JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeWrite(
        JNIEnv* env, jobject thiz, jlong jengine, jlong streamId,
        jbyteArray jdata, jboolean fin) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (!engine) return -1;

    jbyte* data = env->GetByteArrayElements(jdata, nullptr);
    jsize len = env->GetArrayLength(jdata);

    ssize_t written = engine->write(streamId,
                                     reinterpret_cast<const uint8_t*>(data),
                                     len, fin);

    env->ReleaseByteArrayElements(jdata, data, JNI_ABORT);
    return static_cast<jint>(written);
}

// 读取数据
JNIEXPORT jbyteArray JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeRead(
        JNIEnv* env, jobject thiz, jlong jengine, jlong streamId) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (!engine) return nullptr;

    uint8_t buffer[65536];
    bool fin = false;

    ssize_t len = engine->read(streamId, buffer, sizeof(buffer), fin);

    if (len > 0) {
        jbyteArray result = env->NewByteArray(len);
        env->SetByteArrayRegion(result, 0, len, reinterpret_cast<const jbyte*>(buffer));
        return result;
    }

    return nullptr;
}

// 关闭
JNIEXPORT void JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeShutdown(
        JNIEnv* env, jobject thiz, jlong jengine) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (!engine) return;

    engine->shutdown(0, "Normal shutdown");
}

// 销毁
JNIEXPORT void JNICALL
Java_com_example_quichedemo_QuicheEngine_nativeDestroy(
        JNIEnv* env, jobject thiz, jlong jengine) {

    auto* engine = reinterpret_cast<QuicheEngine*>(jengine);
    if (engine) {
        delete engine;
        LOGD("Engine destroyed: %p", engine);
    }
}

} // extern "C"
```

### 3. Kotlin 包装类

**QuicheEngine.kt**:
```kotlin
package com.example.quichedemo

import android.util.Log

class QuicheEngine {
    private var nativeHandle: Long = 0
    private var eventListener: EventListener? = null

    companion object {
        init {
            System.loadLibrary("quichedemo")
        }

        private const val TAG = "QuicheEngine"
    }

    // 事件监听器
    interface EventListener {
        fun onConnected(protocol: String)
        fun onClosed()
        fun onReadable(streamId: Long)
        fun onError(message: String)
    }

    // 连接到服务器
    fun connect(host: String, port: Int, listener: EventListener): Boolean {
        this.eventListener = listener

        // 创建引擎
        nativeHandle = nativeCreate(host, port.toString())
        if (nativeHandle == 0L) {
            Log.e(TAG, "Failed to create native engine")
            return false
        }

        // 设置回调
        nativeSetCallback(nativeHandle, object {
            fun onEvent(event: Int, data: String) {
                when (event) {
                    0 -> listener.onConnected(data)  // CONNECTED
                    1 -> listener.onClosed()         // CONNECTION_CLOSED
                    2 -> listener.onReadable(4)      // STREAM_READABLE
                    3 -> listener.onError(data)      // ERROR
                }
            }
        })

        // 启动
        return nativeStart(nativeHandle)
    }

    // 发送数据
    fun send(streamId: Long, data: ByteArray, finish: Boolean = false): Int {
        if (nativeHandle == 0L) return -1
        return nativeWrite(nativeHandle, streamId, data, finish)
    }

    // 接收数据
    fun receive(streamId: Long): ByteArray? {
        if (nativeHandle == 0L) return null
        return nativeRead(nativeHandle, streamId)
    }

    // 关闭连接
    fun shutdown() {
        if (nativeHandle != 0L) {
            nativeShutdown(nativeHandle)
        }
    }

    // 清理资源
    fun destroy() {
        if (nativeHandle != 0L) {
            nativeDestroy(nativeHandle)
            nativeHandle = 0
        }
    }

    // Native methods
    private external fun nativeCreate(host: String, port: String): Long
    private external fun nativeSetCallback(handle: Long, callback: Any)
    private external fun nativeStart(handle: Long): Boolean
    private external fun nativeWrite(handle: Long, streamId: Long, data: ByteArray, fin: Boolean): Int
    private external fun nativeRead(handle: Long, streamId: Long): ByteArray?
    private external fun nativeShutdown(handle: Long)
    private external fun nativeDestroy(handle: Long)
}
```

### 4. 使用示例

**MainActivity.kt**:
```kotlin
package com.example.quichedemo

import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.*

class MainActivity : AppCompatActivity() {
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private lateinit var engine: QuicheEngine

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        engine = QuicheEngine()
        connectToServer()
    }

    private fun connectToServer() {
        scope.launch {
            val success = engine.connect("example.com", 443, object : QuicheEngine.EventListener {
                override fun onConnected(protocol: String) {
                    Log.d(TAG, "✓ Connected with $protocol")
                    sendRequest()
                }

                override fun onClosed() {
                    Log.d(TAG, "✓ Connection closed")
                }

                override fun onReadable(streamId: Long) {
                    Log.d(TAG, "✓ Stream $streamId has data")
                    receiveResponse(streamId)
                }

                override fun onError(message: String) {
                    Log.e(TAG, "✗ Error: $message")
                }
            })

            if (success) {
                Log.d(TAG, "✓ Engine started")
            } else {
                Log.e(TAG, "✗ Failed to start engine")
            }
        }
    }

    private fun sendRequest() {
        val request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
        val data = request.toByteArray(Charsets.UTF_8)

        val sent = engine.send(4, data)
        Log.d(TAG, "✓ Sent $sent bytes")
    }

    private fun receiveResponse(streamId: Long) {
        val data = engine.receive(streamId)
        if (data != null) {
            val response = String(data, Charsets.UTF_8)
            Log.d(TAG, "✓ Received: ${response.take(100)}...")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.shutdown()
        engine.destroy()
        scope.cancel()
    }

    companion object {
        private const val TAG = "MainActivity"
    }
}
```

## 📊 对比总结

| 特性 | iOS | Android |
|------|-----|---------|
| **库类型** | 静态库 (.a) | 动态库 (.so) |
| **加载方式** | 链接时静态链接 | 运行时动态加载 |
| **C++ 运行时** | 静态链接 libc++ | 需要 libc++_shared.so |
| **包装层** | Swift + Bridging Header | Kotlin + JNI |
| **线程模型** | 主线程 + 事件循环线程 | 相同 |
| **回调机制** | C 函数指针 -> Swift closure | JNI callback -> Kotlin interface |

## 🎯 最佳实践

1. **错误处理**：总是检查返回值并处理错误
2. **资源清理**：确保在适当时机调用 shutdown 和 destroy
3. **线程安全**：引擎是线程安全的，可以从任何线程调用
4. **内存管理**：注意 JNI 中的本地引用和全局引用管理
5. **测试**：在真机上测试所有目标架构

---

*Last updated: 2025-11-06*
