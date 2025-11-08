#ifndef __QUICHE_ENGINE_H__
#define __QUICHE_ENGINE_H__

#include <string>
#include <map>
#include <functional>
#include <cstdint>
#include <cstddef>

extern "C" {
#include <quiche.h>
}

namespace quiche {

// Configuration keys
enum class ConfigKey {
    MAX_IDLE_TIMEOUT,                    // uint64_t: Idle timeout in milliseconds
    MAX_UDP_PAYLOAD_SIZE,                // uint64_t: Max UDP payload size in bytes
    INITIAL_MAX_DATA,                    // uint64_t: Initial max data in bytes
    INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,  // uint64_t: Initial max stream data (bidi local)
    INITIAL_MAX_STREAM_DATA_BIDI_REMOTE, // uint64_t: Initial max stream data (bidi remote)
    INITIAL_MAX_STREAM_DATA_UNI,         // uint64_t: Initial max stream data (uni)
    INITIAL_MAX_STREAMS_BIDI,            // uint64_t: Initial max streams (bidi)
    INITIAL_MAX_STREAMS_UNI,             // uint64_t: Initial max streams (uni)
    DISABLE_ACTIVE_MIGRATION,            // bool: Disable active migration
    ENABLE_DEBUG_LOG,                    // bool: Enable debug logging
};

// Configuration value types (C++11 compatible)
enum class ConfigValueType {
    UINT64,
    BOOL,
    STRING
};

struct ConfigValue {
    ConfigValueType type;
    union {
        uint64_t uint_val;
        bool bool_val;
    };
    std::string str_val;

    ConfigValue() : type(ConfigValueType::UINT64), uint_val(0) {}
    ConfigValue(uint64_t v) : type(ConfigValueType::UINT64), uint_val(v) {}
    ConfigValue(bool v) : type(ConfigValueType::BOOL), bool_val(v) {}
    ConfigValue(const std::string& v) : type(ConfigValueType::STRING), uint_val(0), str_val(v) {}
    ConfigValue(const char* v) : type(ConfigValueType::STRING), uint_val(0), str_val(v) {}
};

using ConfigMap = std::map<ConfigKey, ConfigValue>;

// Engine events
enum class EngineEvent {
    CONNECTED,
    CONNECTION_CLOSED,
    STREAM_READABLE,
    STREAM_WRITABLE,
    DATAGRAM_RECEIVED,
    ERROR,
};

// Event data types (C++11 compatible)
enum class EventDataType {
    NONE,
    STRING,
    UINT64
};

struct EventData {
    EventDataType type;
    std::string str_val;
    uint64_t uint_val;

    EventData() : type(EventDataType::NONE), uint_val(0) {}
    EventData(const std::string& v) : type(EventDataType::STRING), str_val(v), uint_val(0) {}
    EventData(uint64_t v) : type(EventDataType::UINT64), uint_val(v) {}
};

// Connection statistics
struct EngineStats {
    size_t packets_sent;
    size_t packets_received;
    size_t bytes_sent;
    size_t bytes_received;
    size_t packets_lost;
    uint64_t rtt_ns;
    uint64_t cwnd;
};

// Forward declarations
class QuicheEngine;
class QuicheEngineImpl;

// Event callback type
using EventCallback = std::function<void(
    QuicheEngine* engine,
    EngineEvent event,
    const EventData& event_data,
    void* user_data
)>;

/**
 * QUIC Engine - C++ wrapper around quiche library
 * Thread-safe design with background event loop
 */
class QuicheEngine {
public:
    /**
     * Default constructor - creates empty engine object
     * Does not allocate resources or establish connection
     */
    QuicheEngine();

    /**
     * Destructor - automatically closes connection and frees resources
     */
    ~QuicheEngine();

    // Disable copy
    QuicheEngine(const QuicheEngine&) = delete;
    QuicheEngine& operator=(const QuicheEngine&) = delete;

    // Enable move
    QuicheEngine(QuicheEngine&& other) noexcept;
    QuicheEngine& operator=(QuicheEngine&& other) noexcept;

    /**
     * Open engine with QUIC configuration
     * Can be called before or after setEventCallback()
     * Must be called before connect()
     *
     * @param config QUIC configuration parameters
     * @return true on success, false on failure
     *
     * Configuration keys (ConfigKey enum):
     *   - MAX_IDLE_TIMEOUT (uint64_t): Idle timeout in milliseconds (default: 5000)
     *   - MAX_UDP_PAYLOAD_SIZE (uint64_t): Max UDP payload size in bytes (default: 1350)
     *   - INITIAL_MAX_DATA (uint64_t): Initial max data in bytes (default: 10000000)
     *   - INITIAL_MAX_STREAM_DATA_BIDI_LOCAL (uint64_t): Bytes (default: 1000000)
     *   - INITIAL_MAX_STREAM_DATA_BIDI_REMOTE (uint64_t): Bytes (default: 1000000)
     *   - INITIAL_MAX_STREAM_DATA_UNI (uint64_t): Bytes (default: 1000000)
     *   - INITIAL_MAX_STREAMS_BIDI (uint64_t): Stream count (default: 100)
     *   - INITIAL_MAX_STREAMS_UNI (uint64_t): Stream count (default: 100)
     *   - DISABLE_ACTIVE_MIGRATION (bool): Disable migration (default: true)
     *   - ENABLE_DEBUG_LOG (bool): Enable debug logging (default: false)
     */
    bool open(const ConfigMap& config);

    /**
     * Set event callback handler
     * Can be called before or after open()
     * Must be called before connect()
     *
     * @param callback Event callback function
     * @param user_data User data pointer (optional)
     * @return true on success, false on failure
     */
    bool setEventCallback(EventCallback callback, void* user_data = nullptr);

    /**
     * Synchronously connect to server (blocks until connected or timeout)
     *
     * Prerequisites:
     * - Must call open() first to set configuration
     * - Must call setEventCallback() first to set callback
     *
     * @param host Server hostname or IP address
     * @param port Server port number
     * @param timeout_ms Timeout in milliseconds (0 = use default 5000ms)
     * @return Connection ID (8-char hex string) on success, empty string on failure
     *
     * Error handling:
     * - Returns empty if open() not called
     * - Returns empty if setEventCallback() not called
     * - Returns empty on timeout, getLastError() provides details
     * - Returns existing CID if already connected
     */
    std::string connect(const std::string& host, const std::string& port,
                       uint64_t timeout_ms = 5000);

    /**
     * Close connection gracefully (blocking)
     * Sends CONNECTION_CLOSE frame and waits for shutdown
     *
     * @param app_error Application error code (default: 0)
     * @param reason Reason string for close (default: empty)
     *
     * Note: Configuration and callback are preserved after close(),
     *       allowing reconnection with connect()
     */
    void close(uint64_t app_error = 0, const std::string& reason = "");

    /**
     * Write data to stream (thread-safe)
     * Uses internal default stream ID (4)
     *
     * @param data Data buffer
     * @param len Data length
     * @param fin Whether this is the final data on stream
     * @return Number of bytes written, or -1 on error
     */
    ssize_t write(const uint8_t* data, size_t len, bool fin);

    /**
     * Read data from stream (thread-safe)
     * Uses internal default stream ID (4)
     *
     * @param buf Buffer to read into
     * @param buf_len Buffer length
     * @param fin Output: set to true if this is final data
     * @return Number of bytes read, 0 if no data available, -1 on fatal error
     */
    ssize_t read(uint8_t* buf, size_t buf_len, bool& fin);

    /**
     * Check if connection is established
     */
    bool isConnected() const;

    /**
     * Check if event loop is running
     */
    bool isRunning() const;

    /**
     * Get connection statistics
     */
    EngineStats getStats() const;

    /**
     * Get last error message
     */
    std::string getLastError() const;

    /**
     * Get source connection ID (SCID)
     * Returns the unique connection identifier assigned to this connection
     *
     * @return Connection ID as hex string (8 characters)
     */
    std::string getScid() const;

private:
    QuicheEngineImpl* mPImpl;
};

} // namespace quiche

#endif // QUICHE_ENGINE_HPP
