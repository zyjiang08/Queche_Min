// client.cpp
// QUIC Client Demo - Bidirectional Data Transfer Test with Polling
// Cross-platform version using printf for maximum compatibility
//
// Note: Uses printf instead of std::cout to avoid Android bionic libc locale issues
// while maintaining full compatibility with all platforms (macOS, Linux, Android).
//
// Copyright (C) 2025, Cloudflare, Inc.
// All rights reserved.

#include <quiche_engine.h>

#include <cstdio>      // printf instead of cout
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>

using namespace quiche;

// Global state
static std::atomic<bool> connection_ready(false);
static std::atomic<bool> should_stop(false);
static QuicheEngine* global_engine = nullptr;
static std::atomic<uint64_t> total_received(0);

// Data receiving thread - polls for data from server
static void dataReceivingThread() {
    // Wait for connection to be ready
    while (!connection_ready.load() && !should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop.load()) {
        return;
    }

    printf("✓ Starting data reception polling thread...\n");
    fflush(stdout);

    uint8_t buf[65536];  // 64KB buffer
    bool fin = false;

    // Poll for data until connection closes
    while (!should_stop.load()) {
        if (!global_engine) {
            break;
        }

        // Try to read data from stream
        ssize_t len = global_engine->read(buf, sizeof(buf), fin);

        if (len > 0) {
            // Data received
            total_received.fetch_add(len);
            printf("✓ Received %zd bytes from server (total received: %lu bytes)\n",
                   len, (unsigned long)total_received.load());
            fflush(stdout);
        } else if (len == 0) {
            // No data available, continue polling
        } else {
            // Error occurred
            fprintf(stderr, "✗ Read error on stream 4\n");
            fflush(stderr);
            break;
        }

        if (fin) {
            printf("✓ Server stream finished. Total received: %lu bytes\n",
                   (unsigned long)total_received.load());
            fflush(stdout);
            break;
        }

        // Small delay between polls to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Data sending thread - sends 200KB per second
static void dataSendingThread() {
    // Wait for connection to be ready
    while (!connection_ready.load() && !should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop.load()) {
        return;
    }

    printf("✓ Starting data transmission (200KB per second for 5 seconds)...\n");
    fflush(stdout);

    // Prepare 200KB data buffer
    const size_t CHUNK_SIZE = 200 * 1024;  // 200KB per second
    std::vector<uint8_t> data(CHUNK_SIZE);

    // Fill with pattern data
    for (size_t i = 0; i < CHUNK_SIZE; i++) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    uint64_t total_sent = 0;
    int count = 0;

    // Send data every second for 5 seconds
    while (!should_stop.load() && count < 5) {
        auto start = std::chrono::steady_clock::now();

        // Send 200KB on stream 4, in chunks
        if (global_engine) {
            size_t sent_this_round = 0;
            const size_t MAX_CHUNK = 65536;  // 64KB chunks

            for (size_t offset = 0; offset < data.size(); offset += MAX_CHUNK) {
                size_t chunk_size = std::min(MAX_CHUNK, data.size() - offset);

                ssize_t written = global_engine->write(data.data() + offset, chunk_size, false);

                if (written > 0) {
                    sent_this_round += written;
                    total_sent += written;
                } else if (written < 0) {
                    fprintf(stderr, "✗ Failed to send chunk at offset %zu\n", offset);
                    fflush(stderr);
                    break;
                }

                // Small delay between chunks
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            count++;
            printf("✓ Sent %zu bytes in round %d (total sent: %lu bytes)\n",
                   sent_this_round, count, (unsigned long)total_sent);
            fflush(stdout);
        }

        // Wait for 1 second
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto remaining = std::chrono::milliseconds(1000) - elapsed;

        if (remaining.count() > 0) {
            std::this_thread::sleep_for(remaining);
        }
    }

    printf("✓ Data transmission completed. Total sent: %lu bytes\n", (unsigned long)total_sent);
    fflush(stdout);

    // Close the stream with FIN
    if (global_engine) {
        global_engine->write(nullptr, 0, true);  // Send FIN
    }

    printf("\n⏱ Waiting 8 seconds for server to complete sending remaining data...\n");
    fflush(stdout);

    // Wait longer for server to complete sending (8 seconds instead of 2)
    for (int i = 0; i < 8 && !should_stop.load(); i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        printf("  %d/8 seconds...\n", i + 1);
        fflush(stdout);
    }

    should_stop.store(true);
}

// Simplified event callback handler - no STREAM_READABLE event
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
                printf("✓ Connection established: %s\n", event_data.str_val.c_str());
                fflush(stdout);

                // Signal that connection is ready
                connection_ready.store(true);
            } else {
                fprintf(stderr, "✗ Invalid event data for CONNECTED event\n");
                fflush(stderr);
            }
            break;
        }

        case EngineEvent::CONNECTION_CLOSED: {
            printf("✓ Connection closed\n");
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
                printf("  RTT:              %lu ns (%.2f ms)\n",
                       (unsigned long)stats.rtt_ns, stats.rtt_ns / 1000000.0);
                printf("  CWND:             %lu bytes\n", (unsigned long)stats.cwnd);
                printf("\n=== Application Data ===\n");
                printf("  Total received from server: %lu bytes\n",
                       (unsigned long)total_received.load());
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
            // Ignore other events (STREAM_READABLE not used in polling mode)
            break;
    }
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s 127.0.0.1 4433\n", argv[0]);
        fflush(stderr);
        return 1;
    }

    const std::string host = argv[1];
    const std::string port = argv[2];

    printf("QUIC Client Demo - Bidirectional Data Transfer (Polling Mode)\n");
    printf("=============================================================\n");
    printf("Upload:   200KB/sec for 5 seconds\n");
    printf("Download: Polling for data from server\n");
    printf("-------------------------------------------------------------\n");
    printf("Connecting to %s:%s...\n\n", host.c_str(), port.c_str());
    fflush(stdout);

    // Prepare configuration using map with enum keys
    ConfigMap config;

    // QUIC configuration parameters - increased for bidirectional transfer
    config[ConfigKey::MAX_IDLE_TIMEOUT] = static_cast<uint64_t>(30000);          // 30 seconds
    config[ConfigKey::MAX_UDP_PAYLOAD_SIZE] = static_cast<uint64_t>(1350);       // 1350 bytes
    config[ConfigKey::INITIAL_MAX_DATA] = static_cast<uint64_t>(100000000);      // 100MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL] = static_cast<uint64_t>(50000000);   // 50MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE] = static_cast<uint64_t>(50000000);  // 50MB
    config[ConfigKey::INITIAL_MAX_STREAM_DATA_UNI] = static_cast<uint64_t>(10000000);          // 10MB
    config[ConfigKey::INITIAL_MAX_STREAMS_BIDI] = static_cast<uint64_t>(100);    // 100 streams
    config[ConfigKey::INITIAL_MAX_STREAMS_UNI] = static_cast<uint64_t>(100);     // 100 streams
    config[ConfigKey::DISABLE_ACTIVE_MIGRATION] = true;                          // Disable migration
    config[ConfigKey::ENABLE_DEBUG_LOG] = false;                                 // Debug logging off

    try {
        // Initialize engine with configuration map
        QuicheEngine engine(host, port, config);
        global_engine = &engine;

        // Set event callback
        if (!engine.setEventCallback(onEngineEvent, nullptr)) {
            fprintf(stderr, "✗ Failed to set event callback\n");
            fflush(stderr);
            return 1;
        }

        // Start the engine (non-blocking, runs in background thread)
        printf("Starting event loop...\n\n");
        fflush(stdout);
        if (!engine.start()) {
            fprintf(stderr, "\n✗ Engine error: %s\n", engine.getLastError().c_str());
            fflush(stderr);
            return 1;
        }

        // Start data receiving thread (polling mode)
        std::thread receiver_thread(dataReceivingThread);

        // Start data sending thread
        std::thread sender_thread(dataSendingThread);

        // Wait for completion or timeout
        auto start_time = std::chrono::steady_clock::now();
        while (!should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 15) {
                printf("\n⚠ Timeout reached, closing connection...\n");
                fflush(stdout);
                should_stop.store(true);
                break;
            }
        }

        // Wait for threads to finish
        if (receiver_thread.joinable()) {
            receiver_thread.join();
        }
        if (sender_thread.joinable()) {
            sender_thread.join();
        }

        // Print final statistics
        printf("\n");
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n");
        printf("Final Statistics\n");
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n");
        printf("Total received from server: %lu bytes\n", (unsigned long)total_received.load());

        // Get engine statistics
        EngineStats stats = engine.getStats();
        printf("\nConnection Statistics:\n");
        printf("  Packets sent:     %zu\n", stats.packets_sent);
        printf("  Packets received: %zu\n", stats.packets_received);
        printf("  Packets lost:     %zu\n", stats.packets_lost);
        printf("  Bytes sent:       %zu\n", stats.bytes_sent);
        printf("  Bytes received:   %zu\n", stats.bytes_received);
        printf("  RTT:              %.2f ms\n", stats.rtt_ns / 1000000.0);
        printf("  CWND:             %lu bytes\n", (unsigned long)stats.cwnd);
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n");
        fflush(stdout);

        // Shutdown the engine (blocking, waits for graceful shutdown)
        engine.shutdown(0, "Test completed");

        // Clean up (automatic with destructor)
        printf("\nCleaning up...\n");
        fflush(stdout);

        global_engine = nullptr;

    } catch (const std::exception& e) {
        fprintf(stderr, "✗ Exception: %s\n", e.what());
        fflush(stderr);
        return 1;
    }

    printf("✓ Done\n");
    fflush(stdout);
    return 0;
}
