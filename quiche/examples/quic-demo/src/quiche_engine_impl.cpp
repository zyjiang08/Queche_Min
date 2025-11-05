
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
    // std::mutex default constructor - no initialization needed
}

CommandQueue::~CommandQueue() {
    clear();
    // std::mutex destructor called automatically
}

void CommandQueue::push(Command* cmd) {
    cmd->next = nullptr;
    std::lock_guard<std::mutex> lock(mMutex);
    if (tail) {
        tail->next = cmd;
    } else {
        head = cmd;
    }
    tail = cmd;
}

Command* CommandQueue::pop() {
    std::lock_guard<std::mutex> lock(mMutex);
    Command* cmd = head;
    if (cmd) {
        head = cmd->next;
        if (!head) {
            tail = nullptr;
        }
        cmd->next = nullptr;
    }
    return cmd;
}

void CommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    while (head) {
        Command* next = head->next;
        delete head;
        head = next;
    }
    tail = nullptr;
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
    : mHost(h), mPort(p), mConfig(cfg),
      mQuicheCfg(nullptr), mConn(nullptr),
      mSock(-1), mLocalAddrLen(0), mPeerAddrLen(0),
      mLoop(nullptr), mThreadStarted(false),
      mEventCallback(nullptr), mUserData(nullptr), mWrapper(nullptr),
      mIsRunning(false), mIsConnected(false)
{
    memset(&mLocalAddr, 0, sizeof(mLocalAddr));
    memset(&mPeerAddr, 0, sizeof(mPeerAddr));

    // std::mutex default constructor - no initialization needed
}

QuicheEngineImpl::~QuicheEngineImpl() {
    // Stop event mLoop if running
    if (mIsRunning && mLoop) {
        auto* cmd = new Command();
        cmd->type = CommandType::STOP;
        mCmdQueue.push(cmd);
        ev_async_send(mLoop, &mAsyncWatcher);

        if (mThreadStarted) {
            pthread_join(mLoopThread, nullptr);
            mThreadStarted = false;
        }
    }

    // Destroy event loop
    if (mLoop) {
        ev_loop_destroy(mLoop);
        mLoop = nullptr;
    }

    // Free QUIC objects
    if (mConn) {
        quiche_conn_free(mConn);
        mConn = nullptr;
    }

    if (mQuicheCfg) {
        quiche_config_free(mQuicheCfg);
        mQuicheCfg = nullptr;
    }

    // Close socket
    if (mSock >= 0) {
        ::close(mSock);
        mSock = -1;
    }

    // Clean up stream buffers
    {
        std::lock_guard<std::mutex> lock(mStreamBuffersMutex);
        for (auto& pair : mStreamBuffers) {
            delete pair.second;
        }
        mStreamBuffers.clear();
    }
    // std::mutex destructor called automatically
}

// ============================================================================
// Engine::Impl Public Methods
// ============================================================================

bool QuicheEngineImpl::setEventCallback(EventCallback callback, void* ud) {
    mEventCallback = callback;
    mUserData = ud;
    return true;
}

bool QuicheEngineImpl::setupConnection() {
    // Resolve mHostname
    struct addrinfo hints = {};
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* peer;
    if (getaddrinfo(mHost.c_str(), mPort.c_str(), &hints, &peer) != 0) {
        mLastError = "Failed to resolve mHost: " + mHost;
        return false;
    }

    // Save peer address
    mPeerAddrLen = peer->ai_addrlen;
    memcpy(&mPeerAddr, peer->ai_addr, peer->ai_addrlen);

    // Create socket
    mSock = socket(peer->ai_family, SOCK_DGRAM, 0);
    if (mSock < 0) {
        mLastError = "Failed to create socket";
        freeaddrinfo(peer);
        return false;
    }

    // Make mSocket non-blocking
    if (fcntl(mSock, F_SETFL, O_NONBLOCK) != 0) {
        mLastError = "Failed to make mSocket non-blocking";
        ::close(mSock);
        mSock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Create QUIC config
    mQuicheCfg = quiche_config_new(0xbabababa);
    if (!mQuicheCfg) {
        mLastError = "Failed to create QUIC config";
        ::close(mSock);
        mSock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Set application protocols
    quiche_config_set_application_protos(mQuicheCfg,
        (const uint8_t*)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

    // Apply configuration parameters from map
    uint64_t max_idle_timeout = getConfigValue<uint64_t>(ConfigKey::MAX_IDLE_TIMEOUT, 5000);
    quiche_config_set_max_idle_timeout(mQuicheCfg, max_idle_timeout);

    uint64_t max_udp_payload = getConfigValue<uint64_t>(ConfigKey::MAX_UDP_PAYLOAD_SIZE, MAX_DATAGRAM_SIZE);
    quiche_config_set_max_recv_udp_payload_size(mQuicheCfg, max_udp_payload);
    quiche_config_set_max_send_udp_payload_size(mQuicheCfg, max_udp_payload);

    uint64_t initial_max_data = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_DATA, 10000000);
    quiche_config_set_initial_max_data(mQuicheCfg, initial_max_data);

    uint64_t stream_data_bidi_local = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL, 1000000);
    quiche_config_set_initial_max_stream_data_bidi_local(mQuicheCfg, stream_data_bidi_local);

    uint64_t stream_data_bidi_remote = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE, 1000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(mQuicheCfg, stream_data_bidi_remote);

    uint64_t stream_data_uni = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAM_DATA_UNI, 1000000);
    quiche_config_set_initial_max_stream_data_uni(mQuicheCfg, stream_data_uni);

    uint64_t max_streams_bidi = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAMS_BIDI, 100);
    quiche_config_set_initial_max_streams_bidi(mQuicheCfg, max_streams_bidi);

    uint64_t max_streams_uni = getConfigValue<uint64_t>(ConfigKey::INITIAL_MAX_STREAMS_UNI, 100);
    quiche_config_set_initial_max_streams_uni(mQuicheCfg, max_streams_uni);

    bool disable_migration = getConfigValue<bool>(ConfigKey::DISABLE_ACTIVE_MIGRATION, true);
    quiche_config_set_disable_active_migration(mQuicheCfg, disable_migration);

    // Enable SSL key logging if environment variable is set
    if (getenv("SSLKEYLOGFILE")) {
        quiche_config_log_keys(mQuicheCfg);
    }

    // Generate mConnection ID
    uint8_t scid[LOCAL_CONN_ID_LEN];
    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        mLastError = "Failed to open /dev/urandom";
        quiche_config_free(mQuicheCfg);
        mQuicheCfg = nullptr;
        ::close(mSock);
        mSock = -1;
        freeaddrinfo(peer);
        return false;
    }

    ssize_t rand_len = ::read(rng, &scid, sizeof(scid));
    ::close(rng);

    if (rand_len < 0) {
        mLastError = "Failed to generate mConnection ID";
        quiche_config_free(mQuicheCfg);
        mQuicheCfg = nullptr;
        ::close(mSock);
        mSock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Get local address
    mLocalAddrLen = sizeof(mLocalAddr);
    if (getsockname(mSock, (struct sockaddr*)&mLocalAddr, &mLocalAddrLen) != 0) {
        mLastError = "Failed to get local address";
        quiche_config_free(mQuicheCfg);
        mQuicheCfg = nullptr;
        ::close(mSock);
        mSock = -1;
        freeaddrinfo(peer);
        return false;
    }

    // Create QUIC mConnection
    mConn = quiche_connect(mHost.c_str(), scid, sizeof(scid),
                         (struct sockaddr*)&mLocalAddr, mLocalAddrLen,
                         peer->ai_addr, peer->ai_addrlen,
                         mQuicheCfg);

    freeaddrinfo(peer);

    if (!mConn) {
        mLastError = "Failed to create QUIC mConnection";
        quiche_config_free(mQuicheCfg);
        mQuicheCfg = nullptr;
        ::close(mSock);
        mSock = -1;
        return false;
    }

    return true;
}

void QuicheEngineImpl::flushEgress() {
    static uint8_t out[MAX_DATAGRAM_SIZE];

    // No locking needed - called only from event loop thread!

    while (true) {
        quiche_send_info send_info;
        ssize_t written = quiche_conn_send(mConn, out, sizeof(out), &send_info);

        if (written == QUICHE_ERR_DONE) {
            break;
        }

        if (written < 0) {
            mLastError = "Failed to create packet";
            return;
        }

        ssize_t sent = sendto(mSock, out, written, 0,
                             (struct sockaddr*)&send_info.to, send_info.to_len);
        if (sent != written) {
            // Ignore send errors for now
        }
    }

    // Update mTimer
    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(mConn);
    if (timeout_ns != UINT64_MAX) {
        double timeout_sec = (double)timeout_ns / 1000000000.0;
        ev_timer_stop(mLoop, &mTimer);
        ev_timer_set(&mTimer, timeout_sec, 0.0);
        ev_timer_start(mLoop, &mTimer);
    }

    // Check if mConnection is established
    if (quiche_conn_is_established(mConn) && !mIsConnected) {
        mIsConnected = true;

        const uint8_t* app_proto;
        size_t app_proto_len;
        quiche_conn_application_proto(mConn, &app_proto, &app_proto_len);

        if (mEventCallback) {
            std::string proto(reinterpret_cast<const char*>(app_proto), app_proto_len);
            EventData data = proto;
            mEventCallback(nullptr, EngineEvent::CONNECTED, data, mUserData);
        }
    }

    // Check for readable streams and populate buffers
    if (mConn) {
        quiche_stream_iter* readable = quiche_conn_readable(mConn);
        uint64_t stream_id;
        while (quiche_stream_iter_next(readable, &stream_id)) {
            // Read data from quiche into buffer (event loop thread only!)
            readFromQuicheToBuffer(stream_id);

            // Notify application
            if (mEventCallback) {
                EventData data = stream_id;
                mEventCallback(mWrapper, EngineEvent::STREAM_READABLE, data, mUserData);
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
        struct sockaddr_storage mPeerAddr;
        socklen_t mPeerAddrLen = sizeof(mPeerAddr);

        ssize_t len = recvfrom(impl->mSock, buf, sizeof(buf), 0,
                              (struct sockaddr*)&mPeerAddr, &mPeerAddrLen);

        if (len < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            impl->mLastError = "Failed to receive packet";
            break;
        }

        quiche_recv_info recv_info = {
            (struct sockaddr*)&mPeerAddr,
            mPeerAddrLen,
            (struct sockaddr*)&impl->mLocalAddr,
            impl->mLocalAddrLen,
        };

        // No locking needed - called only from event loop thread!
        ssize_t done = quiche_conn_recv(impl->mConn, buf, len, &recv_info);

        if (done < 0) {
            // Ignore receive errors
        }
    }

    impl->flushEgress();

    // No locking needed - called only from event loop thread!
    bool is_closed = quiche_conn_is_closed(impl->mConn);

    if (is_closed) {
        if (impl->mEventCallback) {
            EventData data = std::monostate{};
            impl->mEventCallback(nullptr, EngineEvent::CONNECTION_CLOSED, data, impl->mUserData);
        }
        ev_break(EV_A_ EVBREAK_ONE);
    }
}

void QuicheEngineImpl::timeoutCallback(EV_P_ ev_timer* w, int revents) {
    (void)revents;

    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(w->data);

    // No locking needed - called only from event loop thread!
    quiche_conn_on_timeout(impl->mConn);

    impl->flushEgress();

    // No locking needed - called only from event loop thread!
    bool is_closed = quiche_conn_is_closed(impl->mConn);

    if (is_closed) {
        if (impl->mEventCallback) {
            EventData data = std::monostate{};
            impl->mEventCallback(nullptr, EngineEvent::CONNECTION_CLOSED, data, impl->mUserData);
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
    while ((cmd = mCmdQueue.pop()) != nullptr) {
        switch (cmd->type) {
            case CommandType::WRITE: {
                // No locking needed - called only from event loop thread!
                if (mConn) {
                    uint64_t error_code;
                    ssize_t written = quiche_conn_stream_send(
                        mConn,
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
                // No locking needed - called only from event loop thread!
                if (mConn) {
                    quiche_conn_close(
                        mConn,
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
                ev_break(mLoop, EVBREAK_ONE);
                break;
            }
        }

        delete cmd;
    }
}

void* QuicheEngineImpl::eventLoopThread(void* arg) {
    QuicheEngineImpl* impl = static_cast<QuicheEngineImpl*>(arg);
    ev_run(impl->mLoop, 0);
    impl->mIsRunning = false;
    return nullptr;
}

bool QuicheEngineImpl::start() {
    if (mThreadStarted) {
        mLastError = "Engine already running";
        return false;
    }

    // Enable debug logging if requested
    bool enable_debug = getConfigValue<bool>(ConfigKey::ENABLE_DEBUG_LOG, false);
    if (enable_debug) {
        quiche_enable_debug_logging(debugLog, nullptr);
    }

    // Setup mConnection
    if (!setupConnection()) {
        return false;
    }

    // Create event mLoop
    mLoop = ev_loop_new(EVFLAG_AUTO);
    if (!mLoop) {
        mLastError = "Failed to create event mLoop";
        return false;
    }

    // Initialize IO watcher
    ev_io_init(&mIoWatcher, recvCallback, mSock, EV_READ);
    ev_io_start(mLoop, &mIoWatcher);
    mIoWatcher.data = this;

    // Initialize mTimer
    ev_init(&mTimer, timeoutCallback);
    mTimer.data = this;

    // Initialize async watcher
    ev_async_init(&mAsyncWatcher, asyncCallback);
    ev_async_start(mLoop, &mAsyncWatcher);
    mAsyncWatcher.data = this;

    // Send initial packet
    flushEgress();

    // Start event mLoop in background thread
    mIsRunning = true;
    if (pthread_create(&mLoopThread, nullptr, eventLoopThread, this) != 0) {
        mLastError = "Failed to create event mLoop thread";
        mIsRunning = false;
        ev_loop_destroy(mLoop);
        mLoop = nullptr;
        return false;
    }

    mThreadStarted = true;
    return true;
}

void QuicheEngineImpl::shutdown(uint64_t app_error, const std::string& reason) {
    // Send close command to event mLoop
    if (mIsRunning && mLoop) {
        auto* cmd = new Command();
        cmd->type = CommandType::CLOSE;
        cmd->params.close.error_code = app_error;

        strncpy(cmd->params.close.reason, reason.c_str(), sizeof(cmd->params.close.reason) - 1);
        cmd->params.close.reason[sizeof(cmd->params.close.reason) - 1] = '\0';

        mCmdQueue.push(cmd);
        ev_async_send(mLoop, &mAsyncWatcher);
    }

    // Break event mLoop
    if (mIsRunning && mLoop) {
        ev_break(mLoop, EVBREAK_ONE);
    }

    // Wait for thread to complete
    if (mThreadStarted) {
        pthread_join(mLoopThread, nullptr);
        mThreadStarted = false;
    }

    mIsRunning = false;
}

ssize_t QuicheEngineImpl::write(uint64_t stream_id, const uint8_t* data, size_t len, bool fin) {
    if (!data || len > MAX_WRITE_DATA_SIZE) {
        mLastError = "Invalid write parameters";
        return -1;
    }

    auto* cmd = new Command();
    cmd->type = CommandType::WRITE;
    cmd->params.write.stream_id = stream_id;
    memcpy(cmd->params.write.data, data, len);
    cmd->params.write.len = len;
    cmd->params.write.fin = fin;

    mCmdQueue.push(cmd);

    if (mLoop) {
        ev_async_send(mLoop, &mAsyncWatcher);
    }

    return static_cast<ssize_t>(len);
}

ssize_t QuicheEngineImpl::read(uint64_t stream_id, uint8_t* buf, size_t buf_len, bool& fin) {
    if (!buf) {
        mLastError = "Invalid buffer";
        return -1;
    }

    // Get stream buffer (no quiche calls - lock-free with respect to quiche!)
    StreamReadBuffer* buffer = getOrCreateStreamBuffer(stream_id);

    // Lock buffer access (not mConn - much lighter weight)
    std::lock_guard<std::mutex> lock(buffer->mMutex);

    // Calculate available data
    size_t available = buffer->data.size() - buffer->read_offset;

    if (available == 0) {
        // No data available yet
        fin = buffer->fin_received;
        return 0;
    }

    // Copy data from buffer to output
    size_t to_read = (available < buf_len) ? available : buf_len;
    memcpy(buf, buffer->data.data() + buffer->read_offset, to_read);
    buffer->read_offset += to_read;

    // Check if FIN received and all data consumed
    fin = buffer->fin_received && (buffer->read_offset >= buffer->data.size());

    return static_cast<ssize_t>(to_read);
}


EngineStats QuicheEngineImpl::getStats() const {
    EngineStats stats = {};

    // Note: getStats() is called from application thread, but mConn is only
    // modified in event loop thread. Reading stats is generally safe, but
    // for strict thread safety, we could add a command to get stats from
    // event loop thread. For now, we accept this minor race condition.

    if (mConn) {
        quiche_stats s;
        quiche_conn_stats(mConn, &s);

        stats.packets_sent = s.sent;
        stats.packets_received = s.recv;
        stats.bytes_sent = s.sent_bytes;
        stats.bytes_received = s.recv_bytes;
        stats.packets_lost = s.lost;

        // Get path stats for RTT and CWND (from path 0)
        if (s.paths_count > 0) {
            quiche_path_stats ps;
            if (quiche_conn_path_stats(mConn, 0, &ps) == 0) {
                stats.rtt_ns = ps.rtt;
                stats.cwnd = ps.cwnd;
            }
        }
    }

    return stats;
}

// ============================================================================
// Stream Buffer Helper Methods
// ============================================================================

StreamReadBuffer* QuicheEngineImpl::getOrCreateStreamBuffer(uint64_t stream_id) {
    std::lock_guard<std::mutex> lock(mStreamBuffersMutex);

    auto it = mStreamBuffers.find(stream_id);
    if (it != mStreamBuffers.end()) {
        return it->second;
    }

    // Create new buffer
    StreamReadBuffer* buffer = new StreamReadBuffer();
    mStreamBuffers[stream_id] = buffer;

    return buffer;
}

void QuicheEngineImpl::readFromQuicheToBuffer(uint64_t stream_id) {
    // This is called from event loop thread only - no mConn locking needed!

    StreamReadBuffer* buffer = getOrCreateStreamBuffer(stream_id);

    // Read data from quiche into temporary buffer
    uint8_t temp_buf[65536];
    bool local_fin = false;
    uint64_t error_code;

    ssize_t read_len = quiche_conn_stream_recv(mConn, stream_id, temp_buf,
                                                sizeof(temp_buf), &local_fin, &error_code);

    if (read_len < 0) {
        // Error or no data available
        return;
    }

    // Append to buffer
    {
        std::lock_guard<std::mutex> lock(buffer->mMutex);
        buffer->data.insert(buffer->data.end(), temp_buf, temp_buf + read_len);
        if (local_fin) {
            buffer->fin_received = true;
        }
    }
}

} // namespace quiche
