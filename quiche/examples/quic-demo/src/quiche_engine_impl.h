#ifndef __QUICHE_ENGINE_IMPL_H__
#define __QUICHE_ENGINE_IMPL_H__

#include "quiche_engine.h"

#include <cstring>
#include <memory>

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

// Engine implementation class (PIMPL)
class QuicheEngineImpl {
public:
    QuicheEngineImpl(const std::string& host, const std::string& port, const ConfigMap& config);
    ~QuicheEngineImpl();

    // Disable copy
    QuicheEngineImpl(const QuicheEngineImpl&) = delete;
    QuicheEngineImpl& operator=(const QuicheEngineImpl&) = delete;

    // Public API implementation
    void setWrapper(QuicheEngine* w) { wrapper = w; }
    bool setEventCallback(EventCallback callback, void* user_data);
    ssize_t write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin);
    ssize_t read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin);
    bool run();
    bool join();
    void stop();
    bool close(uint64_t app_error, const std::string& reason);
    bool isConnected() const { return is_connected; }
    bool isRunning() const { return is_running; }
    EngineStats getStats() const;
    std::string getLastError() const { return last_error; }

private:
    // Configuration
    std::string host;
    std::string port;
    ConfigMap config;

    // QUIC objects
    quiche_config* quiche_cfg;
    quiche_conn* conn;

    // Network
    int sock;
    struct sockaddr_storage local_addr;
    socklen_t local_addr_len;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    // Event loop
    struct ev_loop* loop;
    ev_io io_watcher;
    ev_timer timer;
    ev_async async_watcher;
    pthread_t loop_thread;
    bool thread_started;

    // Command queue
    CommandQueue cmd_queue;

    // Callbacks
    EventCallback event_callback;
    void* user_data;
    QuicheEngine* wrapper;  // Pointer back to wrapper for event callbacks

    // State
    bool is_running;
    bool is_connected;
    std::string last_error;

    // Helper methods
    bool setupConnection();
    void flushEgress();
    void processCommands();

    // Static callbacks
    static void* eventLoopThread(void* arg);
    static void recvCallback(EV_P_ ev_io* w, int revents);
    static void timeoutCallback(EV_P_ ev_timer* w, int revents);
    static void asyncCallback(EV_P_ ev_async* w, int revents);
    static void debugLog(const char* line, void* argp);

    // Config helper
    template<typename T>
    T getConfigValue(ConfigKey key, T default_value) const {
        auto it = config.find(key);
        if (it != config.end()) {
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
