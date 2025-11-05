
#include "quiche_engine_impl.h"

#include <iostream>
#include <cstdlib>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
}

namespace quiche {

// ============================================================================
// CommandQueue Implementation
// ============================================================================

CommandQueue::CommandQueue()
    : head(nullptr), tail(nullptr)
{
    pthread_mutex_init(&mutex, nullptr);
}

CommandQueue::~CommandQueue() {
    clear();
    pthread_mutex_destroy(&mutex);
}

void CommandQueue::push(Command* cmd) {
    cmd->next = nullptr;
    pthread_mutex_lock(&mutex);
    if (tail) {
        tail->next = cmd;
    } else {
        head = cmd;
    }
    tail = cmd;
    pthread_mutex_unlock(&mutex);
}

Command* CommandQueue::pop() {
    pthread_mutex_lock(&mutex);
    Command* cmd = head;
    if (cmd) {
        head = cmd->next;
        if (!head) {
            tail = nullptr;
        }
        cmd->next = nullptr;
    }
    pthread_mutex_unlock(&mutex);
    return cmd;
}

void CommandQueue::clear() {
    pthread_mutex_lock(&mutex);
    while (head) {
        Command* next = head->next;
        delete head;
        head = next;
    }
    tail = nullptr;
    pthread_mutex_unlock(&mutex);
}

// ============================================================================
// Engine::Impl Static Callbacks
// ============================================================================

void QuicheEngineImpl::debugLog(const char* line, void* argp) {
    (void)argp;
    std::cerr << "[QUICHE] " << line << std::endl;
}

// ============================================================================
// Engine::Impl Constructor/Destructor
// ============================================================================

QuicheEngineImpl::QuicheEngineImpl(const std::string& h, const std::string& p, const ConfigMap& cfg)
    : host(h), port(p), config(cfg),
      quiche_cfg(nullptr), conn(nullptr),
      sock(-1), local_addr_len(0), peer_addr_len(0),
      loop(nullptr), thread_started(false),
      event_callback(nullptr), user_data(nullptr), wrapper(nullptr),
      is_running(false), is_connected(false)
{
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&peer_addr, 0, sizeof(peer_addr));
}

QuicheEngineImpl::~QuicheEngineImpl() {
    // Stop event loop if running
    if (is_running && loop) {
        auto* cmd = new Command();
        cmd->type = CommandType::STOP;
        cmd_queue.push(cmd);
        ev_async_send(loop, &async_watcher);

        if (thread_started) {
            pthread_join(loop_thread, nullptr);
            thread_started = false;
        }
    }

    // Destroy event loop
    if (loop) {
        ev_loop_destroy(loop);
        loop = nullptr;
    }

    // Free QUIC objects
    if (conn) {
        quiche_conn_free(conn);
        conn = nullptr;
    }

    if (quiche_cfg) {
        quiche_config_free(quiche_cfg);
        quiche_cfg = nullptr;
    }

    // Close socket
    if (sock >= 0) {
        ::close(sock);
        sock = -1;
    }
}

// ============================================================================
// Engine::Impl Public Methods
// ============================================================================

bool QuicheEngineImpl::setEventCallback(EventCallback callback, void* ud) {
    event_callback = callback;
    user_data = ud;
    return true;
}

bool QuicheEngineImpl::setupConnection() {
    // Resolve hostname
    struct addrinfo hints = {};
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* peer;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &peer) != 0) {
        last_error = "Failed to resolve host: " + host;
        return false;
    }

    // Save peer address
    peer_addr_len = peer->ai_addrlen;
    memcpy(&peer_addr, peer->ai_addr, peer->ai_addrlen);

    // Create socket
    sock = socket(peer->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        last_error = "Failed to create socket";
        freeaddrinfo(peer);
        return false;
    }

    // Make socket non-blocking
    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        last_error = "Failed to make socket non-blocking";
        ::close(sock);
        sock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Create QUIC config
    quiche_cfg = quiche_config_new(0xbabababa);
    if (!quiche_cfg) {
        last_error = "Failed to create QUIC config";
        ::close(sock);
        sock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Set application protocols
    quiche_config_set_application_protos(quiche_cfg,
        (const uint8_t*)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

    // Apply configuration parameters from map
    uint64_t max_idle_timeout = getConfigValue<uint64_t>(ConfigKey::MAX_IDLE_TIMEOUT, 5000);
    quiche_config_set_max_idle_timeout(quiche_cfg, max_idle_timeout);

    uint64_t max_udp_payload = getConfigValue<uint64_t>(ConfigKey::MAX_UDP_PAYLOAD_SIZE, MAX_DATAGRAM_SIZE);
    quiche_config_set_max_recv_udp_payload_size(quiche_cfg, max_udp_payload);
    quiche_config_set_max_send_udp_payload_size(quiche_cfg, max_udp_payload);

    uint64_t initial_max_data = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_DATA, 10000000);
    quiche_config_set_initial_max_data(quiche_cfg, initial_max_data);

    uint64_t stream_data_bidi_local = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL, 1000000);
    quiche_config_set_initial_max_stream_data_bidi_local(quiche_cfg, stream_data_bidi_local);

    uint64_t stream_data_bidi_remote = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE, 1000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(quiche_cfg, stream_data_bidi_remote);

    uint64_t stream_data_uni = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_UNI, 1000000);
    quiche_config_set_initial_max_stream_data_uni(quiche_cfg, stream_data_uni);

    uint64_t max_streams_bidi = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAMS_BIDI, 100);
    quiche_config_set_initial_max_streams_bidi(quiche_cfg, max_streams_bidi);

    uint64_t max_streams_uni = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAMS_UNI, 100);
    quiche_config_set_initial_max_streams_uni(quiche_cfg, max_streams_uni);

    bool disable_migration = getConfigValue<bool>(ConfigKey::DISABLE_ACTIVE_MIGRATION, true);
    quiche_config_set_disable_active_migration(quiche_cfg, disable_migration);

    // Enable SSL key logging if environment variable is set
    if (getenv("SSLKEYLOGFILE")) {
        quiche_config_log_keys(quiche_cfg);
    }

    // Generate connection ID
    uint8_t scid[LOCAL_CONN_ID_LEN];
    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        last_error = "Failed to open /dev/urandom";
        quiche_config_free(quiche_cfg);
        quiche_cfg = nullptr;
        ::close(sock);
        sock = -1;
        freeaddrinfo(peer);
        return false;
    }

    ssize_t rand_len = ::read(rng, &scid, sizeof(scid));
    ::close(rng);

    if (rand_len < 0) {
        last_error = "Failed to generate connection ID";
        quiche_config_free(quiche_cfg);
        quiche_cfg = nullptr;
        ::close(sock);
        sock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Get local address
    local_addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr*)&local_addr, &local_addr_len) != 0) {
        last_error = "Failed to get local address";
        quiche_config_free(quiche_cfg);
        quiche_cfg = nullptr;
        ::close(sock);
        sock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Create QUIC connection
    conn = quiche_connect(host.c_str(), scid, sizeof(scid),
                         (struct sockaddr*)&local_addr, local_addr_len,
                         peer->ai_addr, peer->ai_addrlen,
                         quiche_cfg);

    freeaddrinfo(peer);

    if (!conn) {
        last_error = "Failed to create QUIC connection";
        quiche_config_free(quiche_cfg);
        quiche_cfg = nullptr;
        ::close(sock);
        sock = -1;
        return false;
    }

    return true;
}

void QuicheEngineImpl::flushEgress() {
    static uint8_t out[MAX_DATAGRAM_SIZE];

    while (true) {
        quiche_send_info send_info;
        ssize_t written = quiche_conn_send(conn, out, sizeof(out), &send_info);

        if (written == QUICHE_ERR_DONE) {
            break;
        }

        if (written < 0) {
            last_error = "Failed to create packet";
            return;
        }

        ssize_t sent = sendto(sock, out, written, 0,
                             (struct sockaddr*)&send_info.to, send_info.to_len);
        if (sent != written) {
            // Ignore send errors for now
        }
    }

    // Update timer
    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn);
    if (timeout_ns != UINT64_MAX) {
        double timeout_sec = (double)timeout_ns / 1000000000.0;
        ev_timer_stop(loop, &timer);
        ev_timer_set(&timer, timeout_sec, 0.0);
        ev_timer_start(loop, &timer);
    }

    // Check if connection is established
    if (quiche_conn_is_established(conn) && !is_connected) {
        is_connected = true;

        const uint8_t* app_proto;
        size_t app_proto_len;
        quiche_conn_application_proto(conn, &app_proto, &app_proto_len);

        if (event_callback) {
            std::string proto(reinterpret_cast<const char*>(app_proto), app_proto_len);
            EventData data = proto;
            event_callback(nullptr, EngineEvent::CONNECTED, data, user_data);
        }
    }

    // Check for readable streams
    if (conn) {
        quiche_stream_iter* readable = quiche_conn_readable(conn);
        uint64_t stream_id;
        while (quiche_stream_iter_next(readable, &stream_id)) {
            if (event_callback) {
                EventData data = stream_id;
                event_callback(wrapper, EngineEvent::STREAM_READABLE, data, user_data);
            }
        }
        quiche_stream_iter_free(readable);
    }
}

void QuicheEngineImpl::recvCallback(EV_P_ ev_io* w, int revents) {
    (void)EV_A;
    (void)revents;

    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(w->data);
    static uint8_t buf[65535];

    while (true) {
        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        ssize_t len = recvfrom(impl->sock, buf, sizeof(buf), 0,
                              (struct sockaddr*)&peer_addr, &peer_addr_len);

        if (len < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            impl->last_error = "Failed to receive packet";
            break;
        }

        quiche_recv_info recv_info = {
            (struct sockaddr*)&peer_addr,
            peer_addr_len,
            (struct sockaddr*)&impl->local_addr,
            impl->local_addr_len,
        };

        ssize_t done = quiche_conn_recv(impl->conn, buf, len, &recv_info);
        if (done < 0) {
            // Ignore receive errors
        }
    }

    impl->flushEgress();

    if (quiche_conn_is_closed(impl->conn)) {
        if (impl->event_callback) {
            EventData data = std::monostate{};
            impl->event_callback(nullptr, EngineEvent::CONNECTION_CLOSED, data, impl->user_data);
        }
        ev_break(EV_A_ EVBREAK_ONE);
    }
}

void QuicheEngineImpl::timeoutCallback(EV_P_ ev_timer* w, int revents) {
    (void)revents;

    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(w->data);

    quiche_conn_on_timeout(impl->conn);
    impl->flushEgress();

    if (quiche_conn_is_closed(impl->conn)) {
        if (impl->event_callback) {
            EventData data = std::monostate{};
            impl->event_callback(nullptr, EngineEvent::CONNECTION_CLOSED, data, impl->user_data);
        }
        ev_break(EV_A_ EVBREAK_ONE);
    }
}

void QuicheEngineImpl::asyncCallback(EV_P_ ev_async* w, int revents) {
    (void)EV_A;
    (void)revents;

    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(w->data);
    impl->processCommands();
}

void QuicheEngineImpl::processCommands() {
    Command* cmd;
    while ((cmd = cmd_queue.pop()) != nullptr) {
        switch (cmd->type) {
            case CommandType::WRITE: {
                if (conn) {
                    uint64_t error_code;
                    ssize_t written = quiche_conn_stream_send(
                        conn,
                        cmd->params.write.stream_id,
                        cmd->params.write.data,
                        cmd->params.write.len,
                        cmd->params.write.fin,
                        &error_code
                    );

                    if (written < 0) {
                        std::cerr << "[ENGINE] Write failed: error_code=" << error_code << std::endl;
                    }

                    flushEgress();
                }
                break;
            }

            case CommandType::CLOSE: {
                if (conn) {
                    quiche_conn_close(
                        conn,
                        true,
                        cmd->params.close.error_code,
                        reinterpret_cast<const uint8_t*>(cmd->params.close.reason),
                        strlen(cmd->params.close.reason)
                    );
                    flushEgress();
                }
                break;
            }

            case CommandType::STOP: {
                ev_break(loop, EVBREAK_ONE);
                break;
            }
        }

        delete cmd;
    }
}

void* QuicheEngineImpl::eventLoopThread(void* arg) {
    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(arg);
    ev_run(impl->loop, 0);
    impl->is_running = false;
    return nullptr;
}

bool QuicheEngineImpl::run() {
    if (thread_started) {
        last_error = "Engine already running";
        return false;
    }

    // Enable debug logging if requested
    bool enable_debug = getConfigValue<bool>(ConfigKey::ENABLE_DEBUG_LOG, false);
    if (enable_debug) {
        quiche_enable_debug_logging(debugLog, nullptr);
    }

    // Setup connection
    if (!setupConnection()) {
        return false;
    }

    // Create event loop
    loop = ev_loop_new(EVFLAG_AUTO);
    if (!loop) {
        last_error = "Failed to create event loop";
        return false;
    }

    // Initialize IO watcher
    ev_io_init(&io_watcher, recvCallback, sock, EV_READ);
    ev_io_start(loop, &io_watcher);
    io_watcher.data = this;

    // Initialize timer
    ev_init(&timer, timeoutCallback);
    timer.data = this;

    // Initialize async watcher
    ev_async_init(&async_watcher, asyncCallback);
    ev_async_start(loop, &async_watcher);
    async_watcher.data = this;

    // Send initial packet
    flushEgress();

    // Start event loop in background thread
    is_running = true;
    if (pthread_create(&loop_thread, nullptr, eventLoopThread, this) != 0) {
        last_error = "Failed to create event loop thread";
        is_running = false;
        ev_loop_destroy(loop);
        loop = nullptr;
        return false;
    }

    thread_started = true;
    return true;
}

bool QuicheEngineImpl::join() {
    if (!thread_started) {
        return false;
    }

    pthread_join(loop_thread, nullptr);
    thread_started = false;
    return true;
}

void QuicheEngineImpl::stop() {
    if (!is_running || !loop) {
        return;
    }

    ev_break(loop, EVBREAK_ONE);
}

ssize_t QuicheEngineImpl::write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin) {
    if (!data || len > MAX_WRITE_DATA_SIZE) {
        last_error = "Invalid write parameters";
        return -1;
    }

    auto* cmd = new Command();
    cmd->type = CommandType::WRITE;
    cmd->params.write.stream_id = stream_id;
    memcpy(cmd->params.write.data, data, len);
    cmd->params.write.len = len;
    cmd->params.write.fin = fin;

    cmd_queue.push(cmd);

    if (loop) {
        ev_async_send(loop, &async_watcher);
    }

    return static_cast<ssize_t>(len);
}

ssize_t QuicheEngineImpl::read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin) {
    if (!conn || !buf) {
        last_error = "Invalid connection or buffer";
        return -1;
    }

    bool local_fin = false;
    uint64_t error_code;
    ssize_t read_len = quiche_conn_stream_recv(conn, stream_id, buf, buf_len,
                                                &local_fin, &error_code);

    fin = local_fin;

    // Normalize return value:
    // - Positive: actual bytes read
    // - 0: no data available or non-fatal error
    // - -1: fatal error
    if (read_len > 0) {
        return read_len;  // Data read successfully
    } else if (read_len == QUICHE_ERR_DONE) {
        return 0;  // No data available
    } else if (read_len == QUICHE_ERR_INVALID_STREAM_STATE) {
        return 0;  // Stream not ready yet (non-fatal)
    } else {
        // Fatal errors
        last_error = "Stream recv error: " + std::to_string(read_len);
        return -1;
    }
}

bool QuicheEngineImpl::close(uint64_t app_error, const std::string& reason) {
    auto* cmd = new Command();
    cmd->type = CommandType::CLOSE;
    cmd->params.close.error_code = app_error;

    strncpy(cmd->params.close.reason, reason.c_str(), sizeof(cmd->params.close.reason) - 1);
    cmd->params.close.reason[sizeof(cmd->params.close.reason) - 1] = '\0';

    cmd_queue.push(cmd);

    if (loop) {
        ev_async_send(loop, &async_watcher);
    }

    return true;
}

EngineStats QuicheEngineImpl::getStats() const {
    EngineStats stats = {};

    if (conn) {
        quiche_stats s;
        quiche_conn_stats(conn, &s);

        stats.packets_sent = s.sent;
        stats.packets_received = s.recv;
        stats.bytes_sent = s.sent_bytes;
        stats.bytes_received = s.recv_bytes;
        stats.packets_lost = s.lost;

        // Get path stats for RTT and CWND (from path 0)
        if (s.paths_count > 0) {
            quiche_path_stats ps;
            if (quiche_conn_path_stats(conn, 0, &ps) == 0) {
                stats.rtt_ns = ps.rtt;
                stats.cwnd = ps.cwnd;
            }
        }
    }

    return stats;
}

} // namespace quiche
