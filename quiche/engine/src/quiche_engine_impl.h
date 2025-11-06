#ifndef __QUICHE_ENGINE_IMPL_H__
#define __QUICHE_ENGINE_IMPL_H__

#include <quiche_engine.h>

#include <cstring>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <thread>

#include "quiche_thread_utils.h"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <ev.h>
#include <quiche.h>
}

namespace quiche {

// Constants
constexpr size_t LOCAL_CONN_ID_LEN = 16;
constexpr size_t MAX_DATAGRAM_SIZE = 1350;
constexpr size_t MAX_WRITE_DATA_SIZE = 65536;

// Command types for thread-safe communication
enum class CommandType {
    WRITE,
    CLOSE,
    STOP,
};

// Command structure
struct Command {
    CommandType type;

    // Write command data
    struct WriteData {
        uint64_t stream_id;
        uint8_t data[MAX_WRITE_DATA_SIZE];
        size_t len;
        bool fin;
    };

    // Close command data
    struct CloseData {
        uint64_t error_code;
        char reason[256];
    };

    union {
        WriteData write;
        CloseData close;
    } params;

    Command* next;

    Command() : next(nullptr) {}
};

// Command queue (thread-safe FIFO)
class CommandQueue {
public:
    CommandQueue();
    ~CommandQueue();

    // Disable copy
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    void push(Command* cmd);
    Command* pop();
    void clear();

private:
    Command* head;
    Command* tail;
    std::mutex mMutex;  // C++ mutex (non-recursive)
};

// Per-stream read buffer (populated by event loop, read by application threads)
struct StreamReadBuffer {
    std::vector<uint8_t> data;
    size_t read_offset;
    bool fin_received;
    std::mutex mMutex;  // C++ mutex (non-recursive)

    StreamReadBuffer() : read_offset(0), fin_received(false) {}

    ~StreamReadBuffer() = default;

    // Disable copy
    StreamReadBuffer(const StreamReadBuffer&) = delete;
    StreamReadBuffer& operator=(const StreamReadBuffer&) = delete;
};

// Engine implementation class (PIMPL)
class QuicheEngineImpl {
public:
    QuicheEngineImpl(const std::string& host, const std::string& port, const ConfigMap& config);
    ~QuicheEngineImpl();

    // Disable copy
    QuicheEngineImpl(const QuicheEngineImpl&) = delete;
    QuicheEngineImpl& operator=(const QuicheEngineImpl&) = delete;

    // Public API implementation
    void setWrapper(QuicheEngine* w) { mWrapper = w; }
    bool setEventCallback(EventCallback callback, void* user_data);
    ssize_t write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin);
    ssize_t read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin);
    bool start();
    void shutdown(uint64_t app_error, const std::string& reason);
    bool isConnected() const { return mIsConnected; }
    bool isRunning() const { return mIsRunning; }
    EngineStats getStats() const;
    std::string getLastError() const { return mLastError; }

private:
    // Configuration
    std::string mHost;
    std::string mPort;
    ConfigMap mConfig;

    // QUIC objects (accessed only from event loop thread - no locking needed!)
    quiche_config* mQuicheCfg;
    quiche_conn* mConn;

    // Network
    int mSock;
    struct sockaddr_storage mLocalAddr;
    socklen_t mLocalAddrLen;
    struct sockaddr_storage mPeerAddr;
    socklen_t mPeerAddrLen;

    // Event loop
    struct ev_loop* mLoop;
    ev_io mIoWatcher;
    ev_timer mTimer;
    ev_async mAsyncWatcher;
    std::thread mLoopThread;  // C++11 thread (replaces pthread_t)
    bool mThreadStarted;

    // Command queue
    CommandQueue mCmdQueue;

    // Stream read buffers (populated by event loop thread)
    std::map<uint64_t, StreamReadBuffer*> mStreamBuffers;
    std::mutex mStreamBuffersMutex;  // Protect map access (C++ mutex, non-recursive)

    // Callbacks
    EventCallback mEventCallback;
    void* mUserData;
    QuicheEngine* mWrapper;  // Pointer back to wrapper for event callbacks

    // State
    bool mIsRunning;
    bool mIsConnected;
    std::string mLastError;

    // Helper methods
    bool setupConnection();
    void flushEgress();
    void processCommands();
    StreamReadBuffer* getOrCreateStreamBuffer(uint64_t stream_id);
    void readFromQuicheToBuffer(uint64_t stream_id);

    // Static callbacks
    static void eventLoopThread(QuicheEngineImpl* impl);  // C++11 thread function
    static void recvCallback(EV_P_ ev_io* w, int revents);
    static void timeoutCallback(EV_P_ ev_timer* w, int revents);
    static void asyncCallback(EV_P_ ev_async* w, int revents);
    static void debugLog(const char* line, void* argp);

    // Config helper (C++11 compatible)
    template<typename T>
    T getConfigValue(ConfigKey key, T default_value) const {
        return default_value;  // Generic version returns default
    }

    // Template specializations
    uint64_t getConfigValue(ConfigKey key, uint64_t default_value) const {
        auto it = mConfig.find(key);
        if (it != mConfig.end() && it->second.type == ConfigValueType::UINT64) {
            return it->second.uint_val;
        }
        return default_value;
    }

    bool getConfigValue(ConfigKey key, bool default_value) const {
        auto it = mConfig.find(key);
        if (it != mConfig.end() && it->second.type == ConfigValueType::BOOL) {
            return it->second.bool_val;
        }
        return default_value;
    }

    std::string getConfigValue(ConfigKey key, const std::string& default_value) const {
        auto it = mConfig.find(key);
        if (it != mConfig.end() && it->second.type == ConfigValueType::STRING) {
            return it->second.str_val;
        }
        return default_value;
    }
};

} // namespace quiche

#endif // QUICHE_ENGINE_IMPL_HPP
