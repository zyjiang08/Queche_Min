#ifndef __QUICHE_ENGINE_IMPL_H__
#define __QUICHE_ENGINE_IMPL_H__

#include "quiche_engine.h"

#include <cstring>
#include <memory>
#include <map>
#include <vector>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
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
    pthread_mutex_t mutex;
};

// Per-stream read buffer (populated by event loop, read by application threads)
struct StreamReadBuffer {
    std::vector<uint8_t> data;
    size_t read_offset;
    bool fin_received;
    pthread_mutex_t mutex;

    StreamReadBuffer() : read_offset(0), fin_received(false) {
        pthread_mutex_init(&mutex, nullptr);
    }

    ~StreamReadBuffer() {
        pthread_mutex_destroy(&mutex);
    }

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
    pthread_t mLoopThread;
    bool mThreadStarted;

    // Command queue
    CommandQueue mCmdQueue;

    // Stream read buffers (populated by event loop thread)
    std::map<uint64_t, StreamReadBuffer*> mStreamBuffers;
    pthread_mutex_t mStreamBuffersMutex;  // Protect map access

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
    static void* eventLoopThread(void* arg);
    static void recvCallback(EV_P_ ev_io* w, int revents);
    static void timeoutCallback(EV_P_ ev_timer* w, int revents);
    static void asyncCallback(EV_P_ ev_async* w, int revents);
    static void debugLog(const char* line, void* argp);

    // Config helper
    template<typename T>
    T getConfigValue(ConfigKey key, T default_value) const {
        auto it = mConfig.find(key);
        if (it != mConfig.end()) {
            try {
                return std::get<T>(it->second);
            } catch (const std::bad_variant_access&) {
                return default_value;
            }
        }
        return default_value;
    }
};

} // namespace quiche

#endif // QUICHE_ENGINE_IMPL_HPP
