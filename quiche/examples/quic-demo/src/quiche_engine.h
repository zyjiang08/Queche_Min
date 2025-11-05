#ifndef __QUICHE_ENGINE_H__
#define __QUICHE_ENGINE_H__

#include <string>
#include <map>
#include <variant>
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

// Configuration value types
using ConfigValue = std::variant<uint64_t, bool, std::string>;
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

// Event data variant
using EventData = std::variant<
    std::monostate,           // No data
    std::string,              // Protocol string for CONNECTED
    uint64_t                  // Stream ID for STREAM_READABLE/WRITABLE
>;

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
     * Create a new QUIC engine
     *
     * @param host Remote hostname or IP address
     * @param port Remote port number
     * @param config Configuration parameters (optional)
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
    QuicheEngine(const std::string& host, const std::string& port,
                 const ConfigMap& config = ConfigMap());

    /**
     * Destructor - automatically stops and cleans up resources
     */
    ~QuicheEngine();

    // Disable copy
    QuicheEngine(const QuicheEngine&) = delete;
    QuicheEngine& operator=(const QuicheEngine&) = delete;

    // Enable move
    QuicheEngine(QuicheEngine&& other) noexcept;
    QuicheEngine& operator=(QuicheEngine&& other) noexcept;

    /**
     * Set event callback handler
     *
     * @param callback Callback function
     * @param user_data User data passed to callback (optional)
     * @return true on success, false on failure
     */
    bool setEventCallback(EventCallback callback, void* user_data = nullptr);

    /**
     * Write data to a stream (thread-safe)
     *
     * @param stream_id Stream ID
     * @param data Data buffer
     * @param len Data length
     * @param fin Whether this is the final data on stream
     * @return Number of bytes written, or -1 on error
     */
    ssize_t write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin);

    /**
     * Write data to a stream (thread-safe, string overload)
     */
    ssize_t write(uint64_t stream_id, const std::string& data, bool fin);

    /**
     * Read data from a stream
     * Should be called from event callback context
     *
     * @param stream_id Stream ID
     * @param buf Buffer to read into
     * @param buf_len Buffer length
     * @param fin Output: set to true if this is final data
     * @return Number of bytes read, 0 if no data, -1 on error
     */
    ssize_t read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin);

    /**
     * Read data from a stream (string overload)
     */
    std::string read(uint64_t stream_id, bool& fin);

    /**
     * Start the event loop in background thread (non-blocking)
     *
     * @return true on success, false on failure
     */
    bool run();

    /**
     * Wait for event loop thread to finish (blocking)
     *
     * @return true on success, false on failure
     */
    bool join();

    /**
     * Stop the event loop
     */
    void stop();

    /**
     * Close the connection (thread-safe)
     *
     * @param app_error Application error code
     * @param reason Reason string (optional)
     * @return true on success, false on failure
     */
    bool close(uint64_t app_error = 0, const std::string& reason = "");

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

private:
    QuicheEngineImpl* pImpl;
};

} // namespace quiche

#endif // QUICHE_ENGINE_HPP
