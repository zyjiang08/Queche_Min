// http_client.cpp
// HTTP over QUIC Client - File Download Client

#include <quiche_engine.h>
#include <http_protocol.h>

#include <cstdio>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

using namespace quiche;
using namespace http;

// Client configuration
static const char* DEFAULT_OUTPUT_FILE = "download.bin";

// Global state
static std::atomic<bool> connection_ready(false);
static std::atomic<bool> should_stop(false);
static QuicheEngine* global_engine = nullptr;
static std::atomic<uint64_t> total_received(0);
static std::ofstream output_file;
static std::string output_path;
static std::string request_uri;

// Data receiving thread
static void dataReceivingThread() {
    // Wait for connection
    while (!connection_ready.load() && !should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop.load()) {
        return;
    }

    printf("✓ Connection established, sending HTTP GET request...\n");
    fflush(stdout);

    // Build HTTP GET request
    Request request;
    request.method = Method::GET;
    request.uri = request_uri;
    request.version = "HTTP/1.1";
    request.headers["Host"] = "localhost";
    request.headers["User-Agent"] = "HTTP-over-QUIC-Client/1.0";
    request.headers["Accept"] = "*/*";
    request.headers["Connection"] = "close";

    std::string request_str = request.build();

    printf("Sending request:\n%s", request_str.c_str());
    fflush(stdout);

    // Send HTTP request
    if (global_engine) {
        ssize_t sent = global_engine->write(
            reinterpret_cast<const uint8_t*>(request_str.c_str()),
            request_str.length(),
            false
        );

        if (sent > 0) {
            printf("✓ Request sent (%zd bytes)\n\n", sent);
            fflush(stdout);
        } else {
            fprintf(stderr, "✗ Failed to send request\n");
            fflush(stderr);
            should_stop.store(true);
            return;
        }
    }

    // Receive response
    uint8_t buf[65536];  // 64KB buffer
    bool fin = false;
    bool headers_received = false;
    size_t header_end_pos = 0;
    std::string header_buffer;
    std::string expected_sha256;  // Expected SHA256 hash from server
    SHA256_CTX sha256_ctx;  // SHA256 context for verification
    bool sha256_init = false;

    auto start_time = std::chrono::steady_clock::now();
    auto last_report_time = start_time;

    while (!should_stop.load()) {
        if (!global_engine) {
            break;
        }

        // Read data
        ssize_t len = global_engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            total_received.fetch_add(len);

            if (!headers_received) {
                // Accumulate header data
                header_buffer.append(reinterpret_cast<char*>(buf), len);

                // Look for "\r\n\r\n" (end of headers)
                size_t end_pos = header_buffer.find("\r\n\r\n");
                if (end_pos != std::string::npos) {
                    headers_received = true;
                    header_end_pos = end_pos + 4;

                    // Parse HTTP response headers
                    std::string headers_only = header_buffer.substr(0, header_end_pos);
                    Response response;
                    if (response.parse(headers_only)) {
                        printf("✓ HTTP Response received:\n");
                        printf("  Status: %d %s\n",
                               static_cast<int>(response.status_code),
                               response.status_text.c_str());

                        // Print important headers
                        auto it_type = response.headers.find("Content-Type");
                        if (it_type != response.headers.end()) {
                            printf("  Content-Type: %s\n", it_type->second.c_str());
                        }

                        auto it_length = response.headers.find("Content-Length");
                        if (it_length != response.headers.end()) {
                            printf("  Content-Length: %s\n", it_length->second.c_str());
                        }

                        // Extract SHA256 hash if present
                        auto it_sha256 = response.headers.find("X-Content-SHA256");
                        if (it_sha256 != response.headers.end()) {
                            expected_sha256 = it_sha256->second;
                            printf("  X-Content-SHA256: %s\n", expected_sha256.c_str());
                        }

                        printf("\n");
                        fflush(stdout);
                    }

                    // Open output file
                    output_file.open(output_path, std::ios::binary);
                    if (!output_file.is_open()) {
                        fprintf(stderr, "✗ Failed to open output file: %s\n", output_path.c_str());
                        fflush(stderr);
                        should_stop.store(true);
                        return;
                    }

                    printf("✓ Saving to: %s\n", output_path.c_str());

                    // Initialize SHA256 for integrity verification
                    if (!expected_sha256.empty()) {
                        SHA256_Init(&sha256_ctx);
                        sha256_init = true;
                        printf("✓ SHA256 verification enabled\n");
                    }

                    printf("\n");
                    fflush(stdout);

                    // Write body data (if any in first chunk)
                    if (header_buffer.length() > header_end_pos) {
                        size_t body_len = header_buffer.length() - header_end_pos;
                        const char* body_data = header_buffer.c_str() + header_end_pos;
                        output_file.write(body_data, body_len);

                        // Update SHA256 with first chunk
                        if (sha256_init) {
                            SHA256_Update(&sha256_ctx, body_data, body_len);
                        }
                    }

                    header_buffer.clear();  // Free memory
                }
            } else {
                // Write body data
                output_file.write(reinterpret_cast<const char*>(buf), len);

                // Update SHA256 with body data
                if (sha256_init) {
                    SHA256_Update(&sha256_ctx, buf, len);
                }
            }

            // Progress report every second
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_report_time);

            if (elapsed.count() >= 1) {
                auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
                double rate_mbps = 0;
                if (total_elapsed.count() > 0) {
                    rate_mbps = (total_received.load() * 8.0) / (1000000.0 * total_elapsed.count());
                }

                printf("Downloaded: %lu bytes (%.2f MB) | Rate: %.2f Mbps\r",
                       (unsigned long)total_received.load(),
                       total_received.load() / 1048576.0,
                       rate_mbps);
                fflush(stdout);

                last_report_time = now;
            }

        } else if (len == 0) {
            // No data available
        } else {
            // Error
            fprintf(stderr, "\n✗ Read error\n");
            fflush(stderr);
            break;
        }

        if (fin) {
            printf("\n\n✓ Download completed!\n");
            fflush(stdout);
            break;
        }

        // Small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Close output file
    if (output_file.is_open()) {
        output_file.close();
    }

    // Verify SHA256 integrity if enabled
    if (sha256_init && !expected_sha256.empty()) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256_ctx);

        // Convert to hex string
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        std::string calculated_sha256 = ss.str();

        printf("\n=== Integrity Verification ===\n");
        printf("  Expected SHA256:   %s\n", expected_sha256.c_str());
        printf("  Calculated SHA256: %s\n", calculated_sha256.c_str());

        if (calculated_sha256 == expected_sha256) {
            printf("  ✓ Integrity verification PASSED\n");
        } else {
            printf("  ✗ Integrity verification FAILED\n");
        }
        printf("\n");
        fflush(stdout);
    }

    should_stop.store(true);
}

// Event callback
static void onEngineEvent(
    QuicheEngine* engine,
    EngineEvent event,
    const EventData& event_data,
    void* user_data)
{
    (void)user_data;

    switch (event) {
        case EngineEvent::CONNECTED: {
            if (event_data.type == EventDataType::STRING) {
                printf("✓ Connected: %s\n", event_data.str_val.c_str());
                fflush(stdout);
                connection_ready.store(true);
            }
            break;
        }

        case EngineEvent::CONNECTION_CLOSED: {
            printf("\n✓ Connection closed\n");
            fflush(stdout);

            // Print statistics
            if (engine) {
                EngineStats stats = engine->getStats();
                printf("\n=== Connection Statistics ===\n");
                printf("  Packets sent:     %zu\n", stats.packets_sent);
                printf("  Packets received: %zu\n", stats.packets_received);
                printf("  Bytes sent:       %zu\n", stats.bytes_sent);
                printf("  Bytes received:   %zu\n", stats.bytes_received);
                printf("  Packets lost:     %zu\n", stats.packets_lost);
                printf("  RTT:              %.2f ms\n", stats.rtt_ns / 1000000.0);
                printf("  CWND:             %lu bytes\n", (unsigned long)stats.cwnd);
                printf("\n=== Application Statistics ===\n");
                printf("  Total downloaded: %lu bytes (%.2f MB)\n",
                       (unsigned long)total_received.load(),
                       total_received.load() / 1048576.0);
                printf("  Output file:      %s\n", output_path.c_str());
                fflush(stdout);
            }

            should_stop.store(true);
            break;
        }

        case EngineEvent::ERROR: {
            if (engine) {
                fprintf(stderr, "✗ Engine error: %s\n", engine->getLastError().c_str());
                fflush(stderr);
            }
            should_stop.store(true);
            break;
        }

        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <uri> [output_file]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s 127.0.0.1 8443 /test.flv output.flv\n", argv[0]);
        fprintf(stderr, "  %s 127.0.0.1 8443 /data/file.bin download.bin\n", argv[0]);
        fflush(stderr);
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];
    request_uri = argv[3];
    output_path = (argc > 4) ? argv[4] : DEFAULT_OUTPUT_FILE;

    printf("HTTP over QUIC Client\n");
    printf("=====================\n");
    printf("Server:      %s:%s\n", host.c_str(), port.c_str());
    printf("Request URI: %s\n", request_uri.c_str());
    printf("Output file: %s\n", output_path.c_str());
    printf("=====================\n\n");
    fflush(stdout);

    // QUIC configuration
    ConfigMap config;
    config[ConfigKey::MAX_IDLE_TIMEOUT] = static_cast<uint64_t>(300000);  // 5 minutes
    config[ConfigKey::MAX_UDP_PAYLOAD_SIZE] = static_cast<uint64_t>(1350);
    config[ConfigKey::INITIAL_MAX_DATA] = static_cast<uint64_t>(100000000);  // 100MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = static_cast<uint64_t>(50000000);
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE] = static_cast<uint64_t>(50000000);
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_UNI] = static_cast<uint64_t>(10000000);
    config[ConfigKey::INITIAL_MAX_STREAMS_BIDI] = static_cast<uint64_t>(100);
    config[ConfigKey::INITIAL_MAX_STREAMS_UNI] = static_cast<uint64_t>(100);
    config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = true;
    config[ConfigKey::ENABLE_DEBUG_LOG] = false;
    config[ConfigKey::VERIFY_PEER] = false;  // Disable cert verification for self-signed certs

    try {
        // Initialize engine
        QuicheEngine engine;
        global_engine = &engine;

        // Configure
        if (!engine.open(config)) {
            fprintf(stderr, "✗ Failed to open engine\n");
            fflush(stderr);
            return 1;
        }

        // Set callback
        if (!engine.setEventCallback(onEngineEvent, nullptr)) {
            fprintf(stderr, "✗ Failed to set event callback\n");
            fflush(stderr);
            return 1;
        }

        // Connect
        printf("Connecting to %s:%s...\n", host.c_str(), port.c_str());
        fflush(stdout);

        std::string cid = engine.connect(host, port, 10000);  // 10s timeout
        if (cid.empty()) {
            fprintf(stderr, "\n✗ Connection failed: %s\n", engine.getLastError().c_str());
            fflush(stderr);
            return 1;
        }

        // Start download thread
        std::thread download_thread(dataReceivingThread);

        // Wait for completion
        while (!should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for thread
        if (download_thread.joinable()) {
            download_thread.join();
        }

        // Close connection
        printf("\nClosing connection...\n");
        fflush(stdout);
        engine.close(0, "Download complete");

        global_engine = nullptr;

    } catch (const std::exception& e) {
        fprintf(stderr, "✗ Exception: %s\n", e.what());
        fflush(stderr);
        return 1;
    }

    printf("\n✓ Done!\n");
    fflush(stdout);
    return 0;
}
