#ifndef __QUICHE_ENGINE_IMPL_H__
#define __QUICHE_ENGINE_IMPL_H__

#include <quiche_engine.h>

#include <cstring>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
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
constexpr size_t MAX_RECV_BUF_SIZE = 2048;  // Sufficient for receiving any UDP packet
constexpr int BATCH_SIZE = 32;  // Batch size for recvmmsg/sendmmsg
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
    QuicheEngineImpl();  // No-parameter constructor
    ~QuicheEngineImpl();

    // Disable copy
    QuicheEngineImpl(const QuicheEngineImpl&) = delete;
    QuicheEngineImpl& operator=(const QuicheEngineImpl&) = delete;

    // Public API implementation
    void setWrapper(QuicheEngine* w) { mWrapper = w; }
    bool open(const ConfigMap& config);  // New: set configuration
    bool setEventCallback(EventCallback callback, void* user_data);
    std::string connect(const std::string& host, const std::string& port, uint64_t timeout_ms);  // New: synchronous connect
    void close(uint64_t app_error, const std::string& reason);  // Replaces shutdown
    ssize_t write(const uint8_t* data, size_t len, bool fin);
    ssize_t read(uint8_t* buf, size_t buf_len, bool& fin);
    bool isConnected() const { return mIsConnected; }
    bool isRunning() const { return mIsRunning; }
    EngineStats getStats() const;
    std::string getLastError() const { return mLastError; }
    std::string getScid() const { return mScid; }

private:
    // ============ Configuration and callback (managed independently) ============
    ConfigMap mConfig;          // Set by open()
    EventCallback mEventCallback;  // Set by setEventCallback()
    void* mUserData;

    // ============ Connection parameters (set by connect()) ============
    std::string mHost;
    std::string mPort;

    // ============ QUIC objects (accessed only from event loop thread) ============
    quiche_config* mQuicheCfg;
    quiche_conn* mConn;

    // ============ Network resources ============
    int mSock;
    struct sockaddr_storage mLocalAddr;
    socklen_t mLocalAddrLen;
    struct sockaddr_storage mPeerAddr;
    socklen_t mPeerAddrLen;

    // ============ Event loop ============
    struct ev_loop* mLoop;
    ev_io mIoWatcher;
    ev_timer mTimer;
    ev_async mAsyncWatcher;
    std::thread mLoopThread;
    bool mThreadStarted;

    // ============ Command queue ============
    CommandQueue mCmdQueue;

    // ============ Stream read buffers ============
    std::map<uint64_t, StreamReadBuffer*> mStreamBuffers;
    std::mutex mStreamBuffersMutex;

    QuicheEngine* mWrapper;  // Pointer back to wrapper for event callbacks

    // ============ State flags ============
    bool mIsOpened;      // Whether open() has been called
    bool mHasCallback;   // Whether setEventCallback() has been called
    bool mIsConnected;
    bool mIsRunning;
    std::string mLastError;
    std::string mScid;   // Source Connection ID (8-char hex string)
    uint64_t mStreamId;  // Default stream ID for read/write operations

    // ============ Synchronous connection support ============
    std::condition_variable mConnectedCv;  // For connect() to wait on
    std::mutex mConnectedMutex;            // Protects connection state
    bool mConnectComplete;                 // Whether connection attempt completed
    bool mConnectSuccess;                  // Whether connection succeeded

    // I/O buffers (heap memory instead of static to reduce memory footprint)
#if defined(__linux__)
    // Batch I/O buffers for Linux (using recvmmsg/sendmmsg)
    uint8_t (*mSendBufs)[MAX_DATAGRAM_SIZE];     // Array of send buffers
    uint8_t (*mRecvBufs)[MAX_RECV_BUF_SIZE];     // Array of recv buffers
    struct mmsghdr* mSendMsgs;                    // sendmmsg structures
    struct mmsghdr* mRecvMsgs;                    // recvmmsg structures
    struct iovec* mSendIovs;                      // iovec for send
    struct iovec* mRecvIovs;                      // iovec for recv
    quiche_send_info* mSendInfos;                 // send info array
    struct sockaddr_storage* mRecvAddrs;          // peer addresses for recv
#else
    // Single packet buffers for macOS/iOS (using recvmsg/sendmsg)
    uint8_t* mSendBuf;                            // Single send buffer
    uint8_t* mRecvBuf;                            // Single recv buffer
#endif

    // Helper methods
    bool setupConnection();
    bool startEventLoop();  // Start event loop (split from old start())
    void flushEgress();
    void processCommands();
    void notifyConnected(bool success);  // Notify connect() of result
    StreamReadBuffer* getOrCreateStreamBuffer(uint64_t stream_id);
    void readFromQuicheToBuffer(uint64_t stream_id);
    std::string generateRandomHexString();  // Generate 8-char random hex string for SCID

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
